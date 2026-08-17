#include "middleend/pass/transform/mlirvectorize/mlirvectorize.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
namespace {

auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) {
      break;
    }
    value = copy->GetValue();
  }
  return value;
}

auto IsZero(mlir::Value *value) -> bool {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  return constant != nullptr && std::holds_alternative<int>(constant->GetValue()) &&
         std::get<int>(constant->GetValue()) == 0;
}

// A unit-stride array access `root[iv]` recognized from a `var.getptr`.
struct ArrayAccess {
  mlir::Value *root{nullptr};
  bool valid{false};
};

auto MatchArrayAccess(mlir::Value *ptr, mlir::Value *iv) -> ArrayAccess {
  ArrayAccess access;
  ptr = TraceCopies(ptr);
  auto *getptr = ptr == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(ptr->op_);
  if (getptr == nullptr) {
    return access;
  }
  auto indices = getptr->GetIndices();
  if (indices.size() != 1 || TraceCopies(indices[0]) != iv) {
    return access;
  }
  auto *root = TraceCopies(getptr->GetPtr());
  if (root == nullptr) {
    return access;
  }
  // Allow a single array-decay `getptr` (no index) above the access — this arises
  // when a pointer parameter is inlined and its array argument decays to a base
  // pointer. Deeper (2-D / strided) access is not yet handled.
  if (auto *inner = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(root->op_)) {
    if (!inner->GetIndices().empty()) {
      return access;
    }
    root = TraceCopies(inner->GetPtr());
    if (root == nullptr) {
      return access;
    }
  }
  access.root = root;
  access.valid = true;
  return access;
}

// The element-wise operation carried by the intrinsic, encoded as a name suffix.
struct VecOp {
  bool valid{false};
  std::string suffix;  // e.g. "add.i32", "fadd.f32"
  type::Type *elem_type{nullptr};
  mlir::Value *lhs{nullptr};
  mlir::Value *rhs{nullptr};
};

// Map a `var.getval` load (through copies) to the array it reads, at index iv.
auto LoadAccess(mlir::Value *value, mlir::Value *iv) -> ArrayAccess {
  value = TraceCopies(value);
  auto *load = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetValOp>(value->op_);
  return load == nullptr ? ArrayAccess{} : MatchArrayAccess(load->GetPtr(), iv);
}

// Recognize `lhs OP rhs` for the integer/float arithmetic subset we vectorize. The
// element type comes from the operand (a `var.getval` result), whose type is always
// set; the result value's type is not guaranteed at this stage.
template <typename OpTy>
auto TryOp(mlir::Value *value, const char *int_suffix, const char *float_suffix, VecOp *out) -> bool {
  value = TraceCopies(value);
  auto *op = value == nullptr ? nullptr : mlir::DynamicOrNull<OpTy>(value->op_);
  if (op == nullptr) {
    return false;
  }
  auto *lhs = op->GetLHS();
  auto *elem_type = lhs == nullptr ? nullptr : lhs->GetType();
  if (elem_type == nullptr) {
    return false;
  }
  const char *suffix = nullptr;
  switch (elem_type->GetKind()) {
    case type::TypeKind::INT:
      suffix = int_suffix;
      break;
    case type::TypeKind::FLOAT:
      suffix = float_suffix;
      break;
    default:
      return false;
  }
  if (suffix == nullptr) {
    return false;
  }
  out->valid = true;
  out->suffix = suffix;
  out->elem_type = elem_type;
  out->lhs = lhs;
  out->rhs = op->GetRHS();
  return true;
}

auto RecognizeBinary(mlir::Value *value) -> VecOp {
  VecOp result;
  if (TryOp<mlir::arithm::AddOp>(value, "add.i32", "fadd.f32", &result)) return result;
  if (TryOp<mlir::arithm::SubOp>(value, "sub.i32", "fsub.f32", &result)) return result;
  if (TryOp<mlir::arithm::MulOp>(value, "mul.i32", "fmul.f32", &result)) return result;
  if (TryOp<mlir::arithm::DivOp>(value, "div.i32", "fdiv.f32", &result)) return result;
  if (TryOp<mlir::arithm::ModOp>(value, "rem.i32", nullptr, &result)) return result;
  return result;
}

// One vectorizable statement `dst[i] = a[i] OP b[i]`, with all three arrays and
// the op resolved.
struct VecStmt {
  std::string suffix;
  type::Type *elem_type{nullptr};
  mlir::Value *dst_root{nullptr};
  mlir::Value *lhs_root{nullptr};
  mlir::Value *rhs_root{nullptr};
};

auto MatchStore(mlir::var::VarSetOp *store, mlir::Value *iv) -> VecStmt {
  VecStmt stmt;
  auto dst = MatchArrayAccess(store->GetPtr(), iv);
  auto op = RecognizeBinary(store->GetNewValue());
  if (!dst.valid || !op.valid) {
    return stmt;
  }
  auto lhs = LoadAccess(op.lhs, iv);
  auto rhs = LoadAccess(op.rhs, iv);
  if (!lhs.valid || !rhs.valid) {
    return stmt;
  }
  stmt.suffix = op.suffix;
  stmt.elem_type = op.elem_type;
  stmt.dst_root = dst.root;
  stmt.lhs_root = lhs.root;
  stmt.rhs_root = rhs.root;
  return stmt;
}

// Ensure `rvv.<suffix>` is declared as a library intrinsic in the MLIR module so the
// FuncCallOp verifier can resolve it. Registered lazily (after the dead-function
// elimination pass), so it is never swept as an unused declaration.
auto GetOrDeclareIntrinsic(context::MLIRContext *mlir_ctx, const std::string &name,
                           const std::vector<type::Type *> &arg_types, type::Type *return_type) -> void {
  if (mlir_ctx->GetResourceContext()->GetFuncDefOpFromFuncName(name) != nullptr) {
    return;
  }
  std::vector<mlir::Value *> args;
  args.reserve(arg_types.size());
  for (auto *t : arg_types) {
    auto *v = mlir_ctx->CreateValue();
    v->SetType(t);
    args.emplace_back(v);
  }
  auto *def = mlir_ctx->CreateOperation<mlir::func::FuncDefOp>(mlir_ctx, name, args, return_type);
  mlir_ctx->GetModuleOp()->GetBlock()->AddOperation(def);
  mlir_ctx->GetResourceContext()->MapFuncToFuncDefOp(name, def);
}

// Recursively hoist a loop-invariant value (defined inside `loop`) into fresh ops
// appended to `out`. Handles the shapes a loop bound takes: a constant, a load
// (`getval` of a `getptr`), and a `getptr` chain over loop-invariant indices.
// Returns the hoisted value, or nullptr for an unsupported shape (caller bails).
auto HoistValue(context::MLIRContext *mlir_ctx, mlir::Value *value, mlir::scf::WhileOp *loop,
                std::vector<mlir::Operation *> *out) -> mlir::Value * {
  value = TraceCopies(value);
  if (value == nullptr) {
    return nullptr;
  }
  if (!loopiv::IsDefinedInLoop(value, loop)) {
    return value;  // already available outside the loop
  }
  auto *op = value->op_;
  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(op)) {
    auto *ptr = HoistValue(mlir_ctx, load->GetPtr(), loop, out);
    if (ptr == nullptr) {
      return nullptr;
    }
    auto *new_load = mlir_ctx->CreateOperation<mlir::var::VarGetValOp>(mlir_ctx, ptr);
    new_load->GetRes()->SetType(load->GetRes()->GetType());
    out->push_back(new_load);
    return new_load->GetRes();
  }
  if (auto *getptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(op)) {
    auto *ptr = HoistValue(mlir_ctx, getptr->GetPtr(), loop, out);
    if (ptr == nullptr) {
      return nullptr;
    }
    std::vector<mlir::Value *> indices;
    for (auto *idx : getptr->GetIndices()) {
      auto *hoisted = HoistValue(mlir_ctx, idx, loop, out);
      if (hoisted == nullptr) {
        return nullptr;
      }
      indices.push_back(hoisted);
    }
    auto *new_getptr = mlir_ctx->CreateOperation<mlir::var::VarGetPtrOp>(mlir_ctx, ptr, indices);
    new_getptr->GetRes()->SetType(getptr->GetRes()->GetType());
    out->push_back(new_getptr);
    return new_getptr->GetRes();
  }
  if (auto *constant = mlir::DynamicOrNull<mlir::arithm::ConstOp>(op)) {
    auto value_variant = constant->GetValue();
    mlir::arithm::ConstOp *new_const = nullptr;
    if (std::holds_alternative<int>(value_variant)) {
      new_const = mlir_ctx->CreateOperation<mlir::arithm::ConstOp>(mlir_ctx, std::get<int>(value_variant));
    } else {
      new_const = mlir_ctx->CreateOperation<mlir::arithm::ConstOp>(mlir_ctx, std::get<float>(value_variant));
    }
    new_const->GetRes()->SetType(constant->GetRes()->GetType());
    out->push_back(new_const);
    return new_const->GetRes();
  }
  return nullptr;
}

// Turn an array root into its base pointer. A value that is already a pointer (a
// function parameter or a computed base) is used directly; an array symbol is
// decayed via a fresh `var.getptr` (appended to `materialized` so the caller can
// splice it into the block before the intrinsic call).
auto BasePointer(context::MLIRContext *mlir_ctx, context::TypeContext *type_ctx, mlir::Value *root,
                 type::Type *elem_type, std::vector<mlir::Operation *> *materialized) -> mlir::Value * {
  if (root->GetType() != nullptr && root->GetType()->GetKind() == type::TypeKind::PTR) {
    return root;
  }
  auto *getptr = mlir_ctx->CreateOperation<mlir::var::VarGetPtrOp>(mlir_ctx, root, std::vector<mlir::Value *>{});
  getptr->GetRes()->SetType(type_ctx->GetPtrType(elem_type));
  materialized->push_back(getptr);
  return getptr->GetRes();
}

}  // namespace

auto MLIRVectorizePass::Run() -> void {
  PreRun();
  if (std::getenv("VECTORIZE_OFF") != nullptr) {
    return;
  }
  candidates_ = 0;
  vectorized_ = 0;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr && !mlir::func::IsLibraryFunction(function->GetName())) {
      VectorizeInFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "Vectorize: candidates=" << candidates_ << " vectorized=" << vectorized_ << '\n';
  }
}

auto MLIRVectorizePass::VectorizeInFunction(mlir::func::FuncDefOp *function) -> bool {
  return VectorizeInBlock(function, function->GetEntryBlock());
}

auto MLIRVectorizePass::VectorizeInBlock(mlir::func::FuncDefOp *function, mlir::Block *block) -> bool {
  bool changed = false;
  for (auto *operation : block->operations_) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = VectorizeInBlock(function, nested) || changed;
      }
    }
  }
  for (size_t index = 0; index < block->operations_.size(); ++index) {
    if (TryVectorize(block, index)) {
      changed = true;
    }
  }
  return changed;
}

auto MLIRVectorizePass::TryVectorize(mlir::Block *parent, size_t index) -> bool {
  auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(parent->operations_[index]);
  if (loop == nullptr) {
    return false;
  }
  auto induction = loopiv::Recognize(loop);
  // Canonical counted loop: `i < n`, `i += 1`, starting at 0.
  if (!induction.valid_ || induction.kind_ != loopiv::CmpKind::kLT || induction.step_kind_ != loopiv::StepKind::kAdd ||
      induction.step_ != 1 || !IsZero(induction.init_)) {
    return false;
  }
  if (induction.bound_ == nullptr) {
    return false;
  }
  // No externally-live loop results (the induction variable must be dead on exit).
  for (size_t i = 0; i < loop->GetResultsSize(); ++i) {
    if (!loop->GetResultByIndex(i)->GetUses().empty()) {
      return false;
    }
  }
  if (loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *body = loop->GetBodyRegion()->blocks_[0];
  auto *iv = body->arguments_[induction.index_];
  ++candidates_;

  // Collect every store; reject calls / breaks / continues inside the body.
  std::vector<mlir::var::VarSetOp *> stores;
  for (auto *operation : body->operations_) {
    if (mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr) {
      return false;
    }
    if (auto *candidate = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      stores.push_back(candidate);
    }
  }
  if (stores.empty()) {
    return false;
  }

  // Every store must be an independent element-wise `dst[i] = a[i] OP b[i]`.
  std::vector<VecStmt> statements;
  statements.reserve(stores.size());
  for (auto *store : stores) {
    auto stmt = MatchStore(store, iv);
    if (stmt.elem_type == nullptr) {
      return false;
    }
    statements.push_back(stmt);
  }

  auto *int_type = GET_TYPE_CONTEXT()->GetIntegerType();
  auto *void_type = GET_TYPE_CONTEXT()->GetVoidType();

  // Hoist the trip count (loop bound) out of the loop so it can be passed to the
  // intrinsic; bail if its shape is not hoistable.
  std::vector<mlir::Operation *> replacement;
  auto *trip_count = HoistValue(GET_MLIR_CONTEXT(), induction.bound_, loop, &replacement);
  if (trip_count == nullptr) {
    return false;
  }

  // Build the replacement: one base-pointer materialization + intrinsic call per
  // statement, all sharing the loop bound.
  for (const auto &stmt : statements) {
    auto *ptr_type = GET_TYPE_CONTEXT()->GetPtrType(stmt.elem_type);
    auto intrinsic_name = "rvv." + stmt.suffix;
    GetOrDeclareIntrinsic(GET_MLIR_CONTEXT(), intrinsic_name, {int_type, ptr_type, ptr_type, ptr_type}, void_type);
    std::vector<mlir::Operation *> materialized;
    auto *dst_ptr = BasePointer(GET_MLIR_CONTEXT(), GET_TYPE_CONTEXT(), stmt.dst_root, stmt.elem_type, &materialized);
    auto *lhs_ptr = BasePointer(GET_MLIR_CONTEXT(), GET_TYPE_CONTEXT(), stmt.lhs_root, stmt.elem_type, &materialized);
    auto *rhs_ptr = BasePointer(GET_MLIR_CONTEXT(), GET_TYPE_CONTEXT(), stmt.rhs_root, stmt.elem_type, &materialized);
    auto *call = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::FuncCallOp>(
        GET_MLIR_CONTEXT(), intrinsic_name,
        std::vector<mlir::Value *>{trip_count, dst_ptr, lhs_ptr, rhs_ptr}, void_type);
    replacement.insert(replacement.end(), materialized.begin(), materialized.end());
    replacement.push_back(call);
  }

  // Splice the replacement in place of the loop.
  auto it = parent->operations_.begin() + static_cast<std::ptrdiff_t>(index);
  *it = replacement.front();
  parent->operations_.insert(it + 1, replacement.begin() + 1, replacement.end());
  ++vectorized_;
  return true;
}

}  // namespace middleend::pass::transform
