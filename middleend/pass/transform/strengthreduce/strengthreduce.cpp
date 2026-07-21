#include "middleend/pass/transform/strengthreduce/strengthreduce.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/scev/scev.h"
#include "middleend/pass/analysis/scev/scevpass.h"
#include "utils/type/type.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {
auto PtrKey(const void *pointer) -> std::string { return std::to_string(reinterpret_cast<uintptr_t>(pointer)); }

// A recurrence starts at `anchor`; another member of the same family starts at `start`. Their
// difference is the loop-invariant offset that derives one from the other, or nullptr-equivalent zero.
auto IsZeroConstant(const analysis::SCEV *scev) -> bool {
  return scev->GetKind() == analysis::SCEVKind::Constant &&
         static_cast<const analysis::SCEVConstant *>(scev)->IsInt() &&
         static_cast<const analysis::SCEVConstant *>(scev)->GetInt() == 0;
}

// Return the byte distance traversed when one logical GEP index is incremented.
// Array objects and array parameters have slightly different first-index rules:
// an object access has an implicit leading zero in LLVM IR, whereas the first
// index of a pointer parameter advances by its whole pointee type.
auto GetIndexByteStride(type::Type *base_type, size_t index) -> std::optional<int64_t> {
  if (base_type == nullptr) {
    return std::nullopt;
  }

  type::Type *current = base_type;
  if (current->GetKind() == type::TypeKind::PTR) {
    current = static_cast<type::PtrType *>(current)->GetBaseType();
    if (current == nullptr) {
      return std::nullopt;
    }
    if (index == 0) {
      return static_cast<int64_t>(current->GetDataSize());
    }
    --index;
  }

  while (true) {
    if (current == nullptr || current->GetKind() != type::TypeKind::ARRAY) {
      return std::nullopt;
    }
    current = static_cast<type::ArrayType *>(current)->GetElementType();
    if (index == 0) {
      return current == nullptr ? std::nullopt : std::optional<int64_t>(static_cast<int64_t>(current->GetDataSize()));
    }
    --index;
  }
}
}  // namespace

auto StrengthReducePass::Run() -> void {
  PreRun();
  // Benchmark A/B toggle, matching the DISABLE_LICM convention used elsewhere for measuring a pass.
  if (std::getenv("SR_OFF") != nullptr) {
    return;
  }
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *loop_info = GET_PASS_CONTEXT()->GetSpecPass<analysis::LoopInfoPass>("LoopInfoPass");
  scev_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::SCEVPass>("SCEVPass");
  if (cfg_pass == nullptr || loop_info == nullptr || scev_ == nullptr) {
    throw std::logic_error("StrengthReducePass requires CFGPass, LoopInfoPass and SCEVPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("StrengthReducePass only supports MLIRFunctionCFG");
    }

    // A value's defining block is needed to decide whether every use of a candidate stays in the
    // loop; build the map once per function.
    op_to_block_.clear();
    iv_values_.clear();
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        op_to_block_[operation] = mlir_node->GetBlock();
      }
    }

    // Record each header phi that evolves as a recurrence, so a nested reduction can reuse the
    // enclosing loop's induction variable as its stride instead of failing to expand it. A recurrence
    // is uniqued, so the first phi carrying it is a valid representative for any later lookup.
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
        if (phi == nullptr) {
          continue;
        }
        const analysis::SCEV *scev = scev_->GetSCEV(phi->GetRes());
        if (scev->GetKind() == analysis::SCEVKind::AddRec) {
          iv_values_.emplace(scev, phi->GetRes());
        }
      }
    }

    for (auto *loop : loop_info->GetLoops(function_cfg)) {
      changed = RunOnLoop(mlir_function_cfg, loop) || changed;
    }
  }

  if (std::getenv("SR_STATS") != nullptr) {
    std::fprintf(stderr, "[SR] strength-reduced multiplies: %d, getptr addresses: %d\n", reduced_count_,
                 gep_reduced_count_);
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto StrengthReducePass::FindPreheader(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> mlir::Block * {
  auto *header_node = function_cfg->GetNodeByBlock(loop->GetHeader());
  if (header_node == nullptr) {
    return nullptr;
  }
  mlir::Block *preheader = nullptr;
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr) {
      continue;
    }
    auto *pred_block = mlir_pred->GetBlock();
    if (loop->Contains(pred_block)) {
      continue;  // a latch, not the preheader
    }
    if (preheader != nullptr) {
      return nullptr;  // more than one outside predecessor: no dedicated preheader
    }
    preheader = pred_block;
  }
  return preheader;
}

auto StrengthReducePass::RunOnLoop(analysis::MLIRFunctionCFG *function_cfg, analysis::Loop *loop) -> bool {
  auto *preheader = FindPreheader(function_cfg, loop);
  if (preheader == nullptr || preheader->operations_.empty()) {
    return false;
  }
  const auto &latches = loop->GetLatches();
  if (latches.size() != 1) {
    return false;  // a single latch gives the header phi exactly one back-edge value
  }
  auto *latch = latches[0];
  auto *header = loop->GetHeader();
  if (latch->operations_.empty()) {
    return false;
  }

  // The header phi we build has one incoming per header predecessor, so require the canonical shape
  // where those predecessors are exactly the preheader and the latch.
  auto *header_node = function_cfg->GetNodeByBlock(header);
  if (header_node == nullptr || header_node->GetPredecessors().size() != 2) {
    return false;
  }
  for (auto *predecessor : header_node->GetPredecessors()) {
    auto *mlir_pred = dynamic_cast<analysis::MLIRCFGNode *>(predecessor);
    if (mlir_pred == nullptr) {
      return false;
    }
    if (mlir_pred->GetBlock() != preheader && mlir_pred->GetBlock() != latch) {
      return false;
    }
  }

  LoopFrame frame{preheader, latch, header};
  bool changed = false;
  // Sharing is scoped to this loop: one canonical IV per step (per base+indices for pointers), reused
  // by every recurrence that differs only by a loop-invariant offset.
  scalar_families_.clear();
  scalar_members_.clear();
  pointer_families_.clear();
  pointer_members_.clear();
  for (auto *block : loop->GetBlocks()) {
    std::vector<mlir::Operation *> snapshot = block->operations_;
    for (auto *operation : snapshot) {
      if (auto *mul = mlir::DynamicOrNull<mlir::base::MulOp>(operation)) {
        changed = ReduceMul(mul, loop, frame) || changed;
      } else if (auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation)) {
        changed = ReduceGetPtr(get_ptr, loop, frame) || changed;
      }
    }
  }
  return changed;
}

auto StrengthReducePass::ReduceMul(mlir::base::MulOp *mul, analysis::Loop *loop, const LoopFrame &frame) -> bool {
  auto *result = mul->GetRes();
  if (result->GetType() == nullptr || result->GetType()->GetKind() != type::TypeKind::INT) {
    return false;
  }
  const analysis::SCEV *scev = scev_->GetSCEV(result);
  if (scev->GetKind() != analysis::SCEVKind::AddRec) {
    return false;
  }
  const auto *add_rec = static_cast<const analysis::SCEVAddRecExpr *>(scev);
  if (add_rec->GetLoop() != loop || !add_rec->IsAffine()) {
    return false;  // a higher-order chain has no single additive stride to reduce to
  }
  const analysis::SCEV *start = add_rec->GetStart();
  const analysis::SCEV *step = add_rec->GetStep();
  // start/step must be fixed across this loop; an enclosing loop's induction variable qualifies (it
  // is invariant here) and Expand materializes it, so a nested i*j reduces to a walking inner IV.
  if (!scev_->IsLoopInvariant(start, loop) || !scev_->IsLoopInvariant(step, loop)) {
    return false;
  }
  if (!AllUsesInLoop(result, loop)) {
    return false;
  }

  mlir::Value *member = nullptr;
  auto cached = scalar_members_.find(scev);
  if (cached != scalar_members_.end()) {
    member = cached->second;  // an identical recurrence already has a value
  } else {
    auto family = scalar_families_.find(step);
    if (family == scalar_families_.end()) {
      // First recurrence of this step: build the canonical IV {start, +, step}.
      mlir::Value *init = Expand(start, frame.preheader);
      mlir::Value *stride = Expand(step, frame.preheader);
      if (init == nullptr || stride == nullptr) {
        return false;
      }
      member = EmitScalarIV(init, stride, frame, result->GetType());
      scalar_families_[step] = IVFamily{member, start};
    } else {
      // Same step, different start: this value is canonical + (start - anchor), a shared phi plus one
      // invariant offset add -- no extra phi or latch update.
      const analysis::SCEV *delta = scev_->GetMinusSCEV(start, family->second.anchor);
      if (IsZeroConstant(delta)) {
        member = family->second.canonical;
      } else if (mlir::Value *offset = Expand(delta, frame.preheader)) {
        auto *derived = GET_MLIR_CONTEXT()->CreateValue();
        derived->SetType(result->GetType());
        auto *add_op =
            GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(derived, family->second.canonical, offset);
        family->second.canonical->AddUse(add_op, 0);
        offset->AddUse(add_op, 1);
        InsertAfterPhis(add_op, frame.header);
        member = derived;
      } else {
        // The offset could not be materialized; fall back to an independent IV.
        mlir::Value *init = Expand(start, frame.preheader);
        mlir::Value *stride = Expand(step, frame.preheader);
        if (init == nullptr || stride == nullptr) {
          return false;
        }
        member = EmitScalarIV(init, stride, frame, result->GetType());
      }
    }
    scalar_members_[scev] = member;
  }

  result->ReplaceAllUsesWith(member);
  for (auto *operand : mul->operands_) {
    operand->RemoveUse(mul);
  }
  op_to_block_[mul]->RemoveOperation(mul);
  op_to_block_.erase(mul);
  ++reduced_count_;
  return true;
}

auto StrengthReducePass::ReduceGetPtr(mlir::var::VarGetPtrOp *get_ptr, analysis::Loop *loop,
                                      const LoopFrame &frame) -> bool {
  auto *result = get_ptr->GetRes();
  auto *base = get_ptr->GetPtr();
  std::vector<mlir::Value *> indices = get_ptr->GetIndices();
  if (indices.empty() || result->GetType() == nullptr || result->GetType()->GetKind() != type::TypeKind::PTR) {
    return false;
  }
  // Only reduce a full index down to a scalar element. This lets us express an
  // advance in any source dimension as a scalar-element stride. A partial index
  // (for example, getptr(a, i) yielding a row pointer) is left alone because its
  // result does not provide the scalar unit used by the pointer recurrence.
  auto *pointee = static_cast<type::PtrType *>(result->GetType())->GetBaseType();
  if (pointee == nullptr ||
      (pointee->GetKind() != type::TypeKind::INT && pointee->GetKind() != type::TypeKind::FLOAT)) {
    return false;
  }
  // The base must be invariant. Exactly one index may vary in this loop, and it
  // must be an affine recurrence with a compile-time-constant step. Supporting
  // any one varying dimension (rather than only the last) is important for
  // column walks such as a[k][j] in matrix multiplication.
  if (base->op_ != nullptr && op_to_block_.count(base->op_) > 0 && loop->Contains(op_to_block_[base->op_])) {
    return false;
  }
  size_t varying_index = indices.size();
  const analysis::SCEVAddRecExpr *add_rec = nullptr;
  for (size_t i = 0; i < indices.size(); ++i) {
    const analysis::SCEV *index_scev = scev_->GetSCEV(indices[i]);
    if (index_scev->GetKind() == analysis::SCEVKind::AddRec &&
        static_cast<const analysis::SCEVAddRecExpr *>(index_scev)->GetLoop() == loop) {
      if (varying_index != indices.size()) {
        return false;
      }
      varying_index = i;
      add_rec = static_cast<const analysis::SCEVAddRecExpr *>(index_scev);
    } else if (!scev_->IsLoopInvariant(index_scev, loop)) {
      return false;
    }
  }
  if (add_rec == nullptr || !add_rec->IsAffine()) {
    return false;
  }
  // A non-last-dimension pointer IV stays live across the loop. In an outer
  // loop that contains subloops, or for a very high-rank access, this can keep
  // many wide-stride pointers live at once and cause severe register spilling.
  // The intended high-value case is a low-rank innermost column/strided walk,
  // where the address arithmetic is on the hot path and the IV has a short live
  // range.
  if (varying_index + 1 != indices.size() && (!loop->GetSubLoops().empty() || indices.size() > 4)) {
    return false;
  }
  // Preserve the original, deliberately conservative legality rule for the
  // ordinary last-dimension case. SCEV can prove a body-local copy invariant,
  // but rebuilding many such leading indices in a deeply nested loop inflates
  // live ranges and spills. The new relaxed rule is reserved for the previously
  // unsupported non-last-dimension walk.
  if (varying_index + 1 == indices.size()) {
    for (size_t i = 0; i < varying_index; ++i) {
      auto iter = op_to_block_.find(indices[i]->op_);
      if (indices[i]->op_ != nullptr && iter != op_to_block_.end() && loop->Contains(iter->second)) {
        return false;
      }
    }
  }
  const analysis::SCEV *scev = scev_->GetSCEV(indices[varying_index]);
  const analysis::SCEV *start = add_rec->GetStart();
  const analysis::SCEV *step = add_rec->GetStep();
  int step_value = 0;
  if (step->GetKind() != analysis::SCEVKind::Constant || !static_cast<const analysis::SCEVConstant *>(step)->IsInt()) {
    return false;
  }
  step_value = static_cast<const analysis::SCEVConstant *>(step)->GetInt();
  if (step_value == 0 || ContainsAddRec(start) || !scev_->IsLoopInvariant(start, loop)) {
    return false;
  }
  if (!AllUsesInLoop(result, loop)) {
    return false;
  }

  auto byte_stride = GetIndexByteStride(base->GetType(), varying_index);
  int64_t scalar_bytes = static_cast<int64_t>(pointee->GetDataSize());
  if (!byte_stride.has_value() || scalar_bytes <= 0 || *byte_stride <= 0 || *byte_stride % scalar_bytes != 0) {
    return false;
  }
  int64_t dimension_stride = *byte_stride / scalar_bytes;
  int64_t element_step_wide = dimension_stride * static_cast<int64_t>(step_value);
  if (dimension_stride > std::numeric_limits<int>::max() || element_step_wide < std::numeric_limits<int>::min() ||
      element_step_wide > std::numeric_limits<int>::max()) {
    return false;
  }
  int element_step = static_cast<int>(element_step_wide);

  // The initial address is emitted in the preheader. Materialize every index
  // there instead of reusing an invariant value that may have been defined in
  // the loop body (a body-local constant is invariant but does not dominate the
  // preheader).
  std::vector<mlir::Value *> init_indices = indices;
  if (varying_index + 1 == indices.size()) {
    // Keep the established last-dimension path unchanged: the conservative
    // checks above guarantee that every leading value is already available.
    init_indices[varying_index] = Expand(start, frame.preheader);
    if (init_indices[varying_index] == nullptr) {
      return false;
    }
  } else {
    for (size_t i = 0; i < indices.size(); ++i) {
      const analysis::SCEV *initial = i == varying_index ? start : scev_->GetSCEV(indices[i]);
      mlir::Value *materialized = Expand(initial, frame.preheader);
      if (materialized == nullptr) {
        return false;
      }
      init_indices[i] = materialized;
    }
  }

  // Addresses over the same base and leading indices that step by the same amount share one walking
  // pointer: the family key ignores the start, the member key keeps the full recurrence so identical
  // addresses (a read and a write of a[i][j]) still collapse onto one value.
  std::string base_leading = PtrKey(base) + "$" + std::to_string(varying_index);
  for (size_t i = 0; i < indices.size(); ++i) {
    if (i != varying_index) {
      base_leading += ":" + PtrKey(indices[i]);
    }
  }
  std::string family_key = base_leading + "@" + std::to_string(element_step);
  std::string member_key = base_leading + "#" + PtrKey(scev);

  mlir::Value *pointer = nullptr;
  auto cached = pointer_members_.find(member_key);
  if (cached != pointer_members_.end()) {
    pointer = cached->second;
  } else {
    auto family = pointer_families_.find(family_key);
    if (family == pointer_families_.end()) {
      // First address of this family: build the canonical walking pointer at the start index.
      pointer = EmitPointerIV(base, init_indices, element_step, frame, result->GetType());
      pointer_families_[family_key] = IVFamily{pointer, start};
    } else {
      // Same walk, different start: this address is canonical offset by (start - anchor) elements, one
      // getptr of an invariant offset -- no extra pointer phi or latch advance.
      const analysis::SCEV *delta = scev_->GetMinusSCEV(start, family->second.anchor);
      if (IsZeroConstant(delta)) {
        pointer = family->second.canonical;
      } else {
        const analysis::SCEV *element_delta =
            scev_->GetMulExpr({delta, scev_->GetConstant(static_cast<int>(dimension_stride))});
        mlir::Value *offset = Expand(element_delta, frame.preheader);
        if (offset == nullptr) {
          pointer = EmitPointerIV(base, init_indices, element_step, frame, result->GetType());
        } else {
          auto *gep_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
              GET_MLIR_CONTEXT(), family->second.canonical, std::vector<mlir::Value *>{offset});
          gep_op->GetRes()->SetType(result->GetType());
          family->second.canonical->AddUse(gep_op, 0);
          offset->AddUse(gep_op, 1);
          InsertAfterPhis(gep_op, frame.header);
          pointer = gep_op->GetRes();
        }
      }
    }
    pointer_members_[member_key] = pointer;
  }

  result->ReplaceAllUsesWith(pointer);
  for (auto *operand : get_ptr->operands_) {
    operand->RemoveUse(get_ptr);
  }
  op_to_block_[get_ptr]->RemoveOperation(get_ptr);
  op_to_block_.erase(get_ptr);
  ++gep_reduced_count_;
  return true;
}

auto StrengthReducePass::ContainsAddRec(const analysis::SCEV *scev) -> bool {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::AddRec:
      return true;
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      for (const auto *operand : operands) {
        if (ContainsAddRec(operand)) {
          return true;
        }
      }
      return false;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      return ContainsAddRec(div->GetLHS()) || ContainsAddRec(div->GetRHS());
    }
    default:
      return false;
  }
}

auto StrengthReducePass::AllUsesInLoop(mlir::Value *value, analysis::Loop *loop) -> bool {
  for (auto *use : value->GetUses()) {
    auto iter = op_to_block_.find(use->operation_);
    if (iter == op_to_block_.end() || !loop->Contains(iter->second)) {
      return false;
    }
  }
  return true;
}

auto StrengthReducePass::InsertBeforeTerminator(mlir::Operation *operation, mlir::Block *block) -> void {
  block->operations_.insert(block->operations_.end() - 1, operation);
}

auto StrengthReducePass::InsertAfterPhis(mlir::Operation *operation, mlir::Block *block) -> void {
  auto iter = block->operations_.begin();
  while (iter != block->operations_.end() && mlir::DynamicOrNull<mlir::base::PhiOp>(*iter) != nullptr) {
    ++iter;
  }
  block->operations_.insert(iter, operation);
  op_to_block_[operation] = block;
}

auto StrengthReducePass::EmitScalarIV(mlir::Value *init, mlir::Value *stride, const LoopFrame &frame,
                                      type::Type *type) -> mlir::Value * {
  auto *phi_value = GET_MLIR_CONTEXT()->CreateValue();
  phi_value->SetType(type);
  auto *next_value = GET_MLIR_CONTEXT()->CreateValue();
  next_value->SetType(type);
  // Advance value in the latch: next = phi + stride.
  auto *next_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(next_value, phi_value, stride);
  phi_value->AddUse(next_op, 0);
  stride->AddUse(next_op, 1);
  InsertBeforeTerminator(next_op, frame.latch);

  std::vector<mlir::Value *> values = {init, next_value};
  std::vector<mlir::Block *> blocks = {frame.preheader, frame.latch};
  auto *phi_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(phi_value, values, blocks);
  init->AddUse(phi_op, 0);
  next_value->AddUse(phi_op, 1);
  frame.header->operations_.insert(frame.header->operations_.begin(), phi_op);
  op_to_block_[phi_op] = frame.header;
  op_to_block_[next_op] = frame.latch;
  return phi_value;
}

auto StrengthReducePass::EmitPointerIV(mlir::Value *base, const std::vector<mlir::Value *> &init_indices,
                                       int element_step, const LoopFrame &frame, type::Type *type) -> mlir::Value * {
  // Initial pointer in the preheader, with all indices already materialized at
  // a point that dominates the loop header.
  auto *init_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), base, init_indices);
  init_op->GetRes()->SetType(type);
  base->AddUse(init_op, 0);
  for (size_t i = 0; i < init_indices.size(); ++i) {
    init_indices[i]->AddUse(init_op, i + 1);
  }
  InsertBeforeTerminator(init_op, frame.preheader);
  op_to_block_[init_op] = frame.preheader;
  mlir::Value *init_pointer = init_op->GetRes();

  auto *phi_value = GET_MLIR_CONTEXT()->CreateValue();
  phi_value->SetType(type);
  // Latch advance: next = &phi[step], one getptr of `step` elements on the element pointer.
  auto *stride = MakeConstInt(element_step, frame.latch);
  auto *next_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), phi_value,
                                                                              std::vector<mlir::Value *>{stride});
  next_op->GetRes()->SetType(type);
  phi_value->AddUse(next_op, 0);
  stride->AddUse(next_op, 1);
  InsertBeforeTerminator(next_op, frame.latch);
  op_to_block_[next_op] = frame.latch;
  mlir::Value *next_pointer = next_op->GetRes();

  std::vector<mlir::Value *> values = {init_pointer, next_pointer};
  std::vector<mlir::Block *> blocks = {frame.preheader, frame.latch};
  auto *phi_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(phi_value, values, blocks);
  init_pointer->AddUse(phi_op, 0);
  next_pointer->AddUse(phi_op, 1);
  frame.header->operations_.insert(frame.header->operations_.begin(), phi_op);
  op_to_block_[phi_op] = frame.header;
  return phi_value;
}

auto StrengthReducePass::MakeConstInt(int value, mlir::Block *preheader) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(type::TypeFactory::GetIntegerType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, value);
  InsertBeforeTerminator(op, preheader);
  op_to_block_[op] = preheader;
  return result;
}

template <typename BinaryOp>
auto StrengthReducePass::MakeBinary(mlir::Value *lhs, mlir::Value *rhs, mlir::Block *preheader) -> mlir::Value * {
  auto *result = GET_MLIR_CONTEXT()->CreateValue();
  result->SetType(lhs->GetType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<BinaryOp>(result, lhs, rhs);
  lhs->AddUse(op, 0);
  rhs->AddUse(op, 1);
  InsertBeforeTerminator(op, preheader);
  op_to_block_[op] = preheader;
  return result;
}

auto StrengthReducePass::Expand(const analysis::SCEV *scev, mlir::Block *preheader) -> mlir::Value * {
  switch (scev->GetKind()) {
    case analysis::SCEVKind::Constant: {
      const auto *constant = static_cast<const analysis::SCEVConstant *>(scev);
      if (!constant->IsInt()) {
        return nullptr;
      }
      return MakeConstInt(constant->GetInt(), preheader);
    }
    case analysis::SCEVKind::Unknown: {
      // Loop-invariant leaf: it is defined outside the loop and therefore dominates the preheader,
      // so it is directly usable at the insertion point.
      return static_cast<const analysis::SCEVUnknown *>(scev)->GetValue();
    }
    case analysis::SCEVKind::Add:
    case analysis::SCEVKind::Mul: {
      const auto &operands = static_cast<const analysis::SCEVCommutativeExpr *>(scev)->GetOperands();
      mlir::Value *accumulator = nullptr;
      for (const auto *operand : operands) {
        mlir::Value *expanded = Expand(operand, preheader);
        if (expanded == nullptr) {
          return nullptr;
        }
        if (accumulator == nullptr) {
          accumulator = expanded;
        } else if (scev->GetKind() == analysis::SCEVKind::Add) {
          accumulator = MakeBinary<mlir::base::AddOp>(accumulator, expanded, preheader);
        } else {
          accumulator = MakeBinary<mlir::base::MulOp>(accumulator, expanded, preheader);
        }
      }
      return accumulator;
    }
    case analysis::SCEVKind::Div: {
      const auto *div = static_cast<const analysis::SCEVDivExpr *>(scev);
      mlir::Value *lhs = Expand(div->GetLHS(), preheader);
      mlir::Value *rhs = Expand(div->GetRHS(), preheader);
      if (lhs == nullptr || rhs == nullptr) {
        return nullptr;
      }
      return MakeBinary<mlir::base::DivOp>(lhs, rhs, preheader);
    }
    case analysis::SCEVKind::AddRec: {
      // The only recurrence expandable here is an enclosing loop's induction variable, reused as its
      // existing header phi. Requiring that loop to contain the insertion block guarantees the phi
      // dominates it; anything else (or an unrecorded recurrence) cannot be materialized.
      const auto *add_rec = static_cast<const analysis::SCEVAddRecExpr *>(scev);
      if (!add_rec->GetLoop()->Contains(preheader)) {
        return nullptr;
      }
      auto iter = iv_values_.find(scev);
      return iter == iv_values_.end() ? nullptr : iter->second;
    }
    default:
      return nullptr;
  }
}

}  // namespace middleend::pass::transform
