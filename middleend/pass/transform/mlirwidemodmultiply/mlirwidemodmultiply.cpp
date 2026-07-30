#include "middleend/pass/transform/mlirwidemodmultiply/mlirwidemodmultiply.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <variant>
#include <vector>

#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/scf.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

auto Trace(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) break;
    value = copy->GetValue();
  }
  return value;
}

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool { return Trace(lhs) == Trace(rhs); }

auto ConstInt(mlir::Value *value, int *result) -> bool {
  value = Trace(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) return false;
  *result = std::get<int>(constant->GetValue());
  return true;
}

auto IsRemainderBy(mlir::Value *value, mlir::Value *dividend, int divisor) -> bool {
  value = Trace(value);
  auto *mod = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ModOp>(value->op_);
  int actual_divisor = 0;
  return mod != nullptr && Same(mod->GetLHS(), dividend) && ConstInt(mod->GetRHS(), &actual_divisor) &&
         actual_divisor == divisor;
}

auto EqualityWithInt(mlir::Value *condition, mlir::Value *value, int expected) -> bool {
  condition = Trace(condition);
  auto *equal = condition == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::EqualOp>(condition->op_);
  int constant = 0;
  return equal != nullptr &&
         ((Same(equal->GetLHS(), value) && ConstInt(equal->GetRHS(), &constant) && constant == expected) ||
          (Same(equal->GetRHS(), value) && ConstInt(equal->GetLHS(), &constant) && constant == expected));
}

auto IsOddEquality(mlir::Value *condition, mlir::Value *value) -> bool {
  condition = Trace(condition);
  auto *equal = condition == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::EqualOp>(condition->op_);
  int constant = 0;
  return equal != nullptr &&
         ((IsRemainderBy(equal->GetLHS(), value, 2) && ConstInt(equal->GetRHS(), &constant) && constant == 1) ||
          (IsRemainderBy(equal->GetRHS(), value, 2) && ConstInt(equal->GetLHS(), &constant) && constant == 1));
}

auto RegionReturnsValue(mlir::Region *region, mlir::Value *expected) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation);
          return_op != nullptr && Same(return_op->GetReturnValue(), expected)) {
        return true;
      }
      for (auto *nested : operation->regions_) {
        if (RegionReturnsValue(nested, expected)) return true;
      }
    }
  }
  return false;
}

auto RegionReturnsInt(mlir::Region *region, int expected) -> bool {
  for (auto *block : region->blocks_) {
    for (auto *operation : block->operations_) {
      if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
        int constant = 0;
        if (ConstInt(return_op->GetReturnValue(), &constant) && constant == expected) return true;
      }
      for (auto *nested : operation->regions_) {
        if (RegionReturnsInt(nested, expected)) return true;
      }
    }
  }
  return false;
}

auto GlobalConstInt(mlir::Value *pointer, int *result) -> bool {
  auto root = analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(pointer);
  auto *global = root.value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(root.value->op_);
  if (root.kind != analysis::MLIRMemoryEffectAnalysis::RootKind::kGlobal || global == nullptr || !global->IsConst() ||
      !global->GetDimensions().empty() || global->GetIntInitValues().size() != 1) {
    return false;
  }
  *result = global->GetIntInitValues()[0];
  return true;
}

auto MakeRegion(context::MLIRContext *context, const std::vector<mlir::Operation *> &operations,
                mlir::Value *value) -> mlir::Region * {
  auto *block = new mlir::Block();
  for (auto *operation : operations) block->operations_.emplace_back(operation);
  block->operations_.emplace_back(context->CreateOperation<mlir::scf::YieldOp>(context, value));
  auto *region = new mlir::Region();
  region->blocks_.emplace_back(block);
  return region;
}

auto CollectOperations(mlir::Operation *operation, std::vector<mlir::Operation *> *operations) -> void {
  operations->emplace_back(operation);
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) CollectOperations(nested, operations);
    }
  }
}

auto CollectCallsInBlock(mlir::func::FuncDefOp *caller, mlir::Block *block, const std::string &name,
                         std::vector<MLIRWideModMultiplyPass::CallSite> *sites) -> void {
  for (auto *operation : block->operations_) {
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation);
        call != nullptr && call->GetName() == name) {
      sites->push_back({caller, block, call});
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) CollectCallsInBlock(caller, nested, name, sites);
    }
  }
}

}  // namespace

auto MLIRWideModMultiplyPass::Run() -> void {
  PreRun();
  functions_ = 0;
  candidates_ = 0;
  calls_rewritten_ = 0;
  if (std::getenv("WIDE_MOD_MULTIPLY_OFF") == nullptr) {
    auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
    std::vector<Candidate> candidates;
    for (auto *operation : module->GetBlock()->operations_) {
      auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
      if (function == nullptr) continue;
      ++functions_;
      Candidate candidate;
      if (FindCandidate(function, &candidate)) candidates.emplace_back(candidate);
    }
    candidates_ = candidates.size();
    for (const auto &candidate : candidates) {
      std::vector<CallSite> sites;
      CollectCallSites(candidate, &sites);
      for (const auto &site : sites) {
        RewriteCall(candidate, site);
        ++calls_rewritten_;
      }
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr || std::getenv("WIDE_MOD_MULTIPLY_STATS") != nullptr) {
    std::cerr << "WideModMultiply: functions=" << functions_ << " candidates=" << candidates_
              << " calls=" << calls_rewritten_ << '\n';
  }
}

auto MLIRWideModMultiplyPass::FindCandidate(mlir::func::FuncDefOp *function, Candidate *candidate) -> bool {
  auto arguments = function->GetArguments();
  if (arguments.size() != 2 || arguments[0]->GetType()->GetKind() != type::TypeKind::INT ||
      arguments[1]->GetType()->GetKind() != type::TypeKind::INT) {
    return false;
  }
  std::vector<mlir::Operation *> operations;
  for (auto *operation : function->GetEntryBlock()->operations_) CollectOperations(operation, &operations);
  mlir::func::FuncCallOp *recursive = nullptr;
  bool base_zero = false;
  bool base_one = false;
  bool odd_test = false;
  int modulus = 0;
  std::vector<mlir::arithm::ModOp *> modular_reductions;
  std::vector<mlir::var::VarGetValOp *> loads;
  std::vector<mlir::func::ReturnOp *> returns;
  for (auto *operation : operations) {
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
      if (call->GetName() != function->GetName() || recursive != nullptr) return false;
      recursive = call;
    }
    if (auto *equal = mlir::DynamicOrNull<mlir::arithm::EqualOp>(operation)) {
      int value = 0;
      if (Same(equal->GetLHS(), arguments[1]) && ConstInt(equal->GetRHS(), &value)) {
        base_zero = base_zero || value == 0;
        base_one = base_one || value == 1;
      }
      if (Same(equal->GetRHS(), arguments[1]) && ConstInt(equal->GetLHS(), &value)) {
        base_zero = base_zero || value == 0;
        base_one = base_one || value == 1;
      }
      odd_test = odd_test ||
                 (IsRemainderBy(equal->GetLHS(), arguments[1], 2) && ConstInt(equal->GetRHS(), &value) && value == 1) ||
                 (IsRemainderBy(equal->GetRHS(), arguments[1], 2) && ConstInt(equal->GetLHS(), &value) && value == 1);
    }
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      loads.emplace_back(load);
    }
    if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr) return false;
    if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation)) {
      returns.emplace_back(return_op);
    }
    if (auto *mod = mlir::DynamicOrNull<mlir::arithm::ModOp>(operation)) {
      auto *rhs = Trace(mod->GetRHS());
      auto *load = rhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetValOp>(rhs->op_);
      // The recurrence also uses b % 2 to select its odd branch. Only loads
      // from scalar const globals describe the arithmetic modulus.
      if (load == nullptr) continue;
      int current = 0;
      if (!GlobalConstInt(load->GetPtr(), &current) || (modulus != 0 && modulus != current)) {
        return false;
      }
      modulus = current;
      modular_reductions.emplace_back(mod);
    }
  }
  if (recursive == nullptr || !base_zero || !base_one || !odd_test || modular_reductions.size() != 3 || modulus <= 1 ||
      modulus > INT_MAX / 2) {
    return false;
  }
  for (auto *load : loads) {
    int loaded_value = 0;
    if (!GlobalConstInt(load->GetPtr(), &loaded_value) || loaded_value != modulus) {
      return false;
    }
  }
  auto recursive_arguments = recursive->GetOperands();
  if (recursive_arguments.size() != 2 || !Same(recursive_arguments[0], arguments[0])) return false;
  auto *traced_exponent = Trace(recursive_arguments[1]);
  auto *division =
      traced_exponent == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::DivOp>(traced_exponent->op_);
  int divisor = 0;
  if (division == nullptr || !Same(division->GetLHS(), arguments[1]) || !ConstInt(division->GetRHS(), &divisor) ||
      divisor != 2) {
    return false;
  }

  mlir::arithm::ModOp *base_mod = nullptr;
  mlir::arithm::ModOp *doubled_mod = nullptr;
  for (auto *mod : modular_reductions) {
    if (Same(mod->GetLHS(), arguments[0])) {
      if (base_mod != nullptr) return false;
      base_mod = mod;
      continue;
    }
    auto *lhs = Trace(mod->GetLHS());
    auto *add = lhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(lhs->op_);
    if (add != nullptr && Same(add->GetLHS(), recursive->GetRes()) && Same(add->GetRHS(), recursive->GetRes())) {
      if (doubled_mod != nullptr) return false;
      doubled_mod = mod;
    }
  }
  if (base_mod == nullptr || doubled_mod == nullptr) {
    return false;
  }

  mlir::arithm::ModOp *odd_mod = nullptr;
  for (auto *mod : modular_reductions) {
    auto *lhs = Trace(mod->GetLHS());
    auto *add = lhs == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::AddOp>(lhs->op_);
    if (add == nullptr) continue;
    bool matches = (Same(add->GetLHS(), doubled_mod->GetRes()) && Same(add->GetRHS(), arguments[0])) ||
                   (Same(add->GetRHS(), doubled_mod->GetRes()) && Same(add->GetLHS(), arguments[0]));
    if (!matches) continue;
    if (odd_mod != nullptr) return false;
    odd_mod = mod;
  }
  if (odd_mod == nullptr) {
    return false;
  }

  bool zero_branch = false;
  bool one_branch = false;
  bool odd_branch = false;
  bool even_fallthrough = false;
  for (auto *operation : function->GetEntryBlock()->operations_) {
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      if (EqualityWithInt(if_op->GetCondition(), arguments[1], 0) && RegionReturnsInt(if_op->GetThenRegion(), 0)) {
        zero_branch = true;
      }
      if (EqualityWithInt(if_op->GetCondition(), arguments[1], 1) &&
          RegionReturnsValue(if_op->GetThenRegion(), base_mod->GetRes())) {
        one_branch = true;
      }
      if (IsOddEquality(if_op->GetCondition(), arguments[1]) &&
          RegionReturnsValue(if_op->GetThenRegion(), odd_mod->GetRes())) {
        odd_branch = true;
        even_fallthrough = even_fallthrough || RegionReturnsValue(if_op->GetElseRegion(), doubled_mod->GetRes());
      }
    }
    if (auto *return_op = mlir::DynamicOrNull<mlir::func::ReturnOp>(operation);
        return_op != nullptr && Same(return_op->GetReturnValue(), doubled_mod->GetRes())) {
      even_fallthrough = true;
    }
  }
  if (!zero_branch || !one_branch || !odd_branch || !even_fallthrough) return false;

  bool returns_base = false;
  bool returns_doubled = false;
  bool returns_odd = false;
  for (auto *return_op : returns) {
    auto *value = return_op->GetReturnValue();
    int constant = 0;
    if (Same(value, base_mod->GetRes())) {
      returns_base = true;
    } else if (Same(value, doubled_mod->GetRes())) {
      returns_doubled = true;
    } else if (Same(value, odd_mod->GetRes())) {
      returns_odd = true;
    } else if (!ConstInt(value, &constant) || constant != 0) {
      return false;
    }
  }
  if (!returns_base || !returns_doubled || !returns_odd) {
    return false;
  }

  candidate->function = function;
  candidate->modulus = modulus;
  return true;
}

auto MLIRWideModMultiplyPass::CollectCallSites(const Candidate &candidate, std::vector<CallSite> *sites) -> void {
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function == nullptr || function == candidate.function) continue;
    CollectCallsInBlock(function, function->GetEntryBlock(), candidate.function->GetName(), sites);
  }
}

auto MLIRWideModMultiplyPass::RewriteCall(const Candidate &candidate, const CallSite &site) -> void {
  auto *context = GET_MLIR_CONTEXT();
  auto arguments = site.call->GetOperands();
  auto *integer_type = arguments[0]->GetType();
  type::Type *bool_type = nullptr;

  auto *zero = context->CreateOperation<mlir::arithm::ConstOp>(context, 0);
  auto *lower = context->CreateOperation<mlir::arithm::ConstOp>(context, -candidate.modulus);
  auto *upper = context->CreateOperation<mlir::arithm::ConstOp>(context, candidate.modulus);
  auto *modulus = context->CreateOperation<mlir::arithm::ConstOp>(context, candidate.modulus);
  for (auto *constant : {zero, lower, upper, modulus}) constant->GetRes()->SetType(integer_type);
  auto *nonnegative = context->CreateOperation<mlir::arithm::GreaterEqualOp>(context, arguments[1], zero->GetRes());
  auto *above_lower = context->CreateOperation<mlir::arithm::GreaterEqualOp>(context, arguments[0], lower->GetRes());
  auto *below_upper = context->CreateOperation<mlir::arithm::LessEqualOp>(context, arguments[0], upper->GetRes());
  // Reuse the nearest comparison result type; all comparisons in this dialect are Bool.
  bool_type = nonnegative->GetRes()->GetType();
  if (bool_type == nullptr) bool_type = compiler_context_->GetTypeContext()->GetBooleanType();
  nonnegative->GetRes()->SetType(bool_type);
  above_lower->GetRes()->SetType(bool_type);
  below_upper->GetRes()->SetType(bool_type);

  auto make_and = [&](mlir::Value *lhs, mlir::Value *rhs) {
    auto *operation = context->CreateOperation<mlir::scf::IfOp>(context, 1, lhs, MakeRegion(context, {}, rhs),
                                                                MakeRegion(context, {}, lhs));
    operation->GetResultByIndex(0)->SetType(bool_type);
    return operation;
  };
  auto *range = make_and(above_lower->GetRes(), below_upper->GetRes());
  auto *safe = make_and(nonnegative->GetRes(), range->GetResultByIndex(0));

  auto *wide =
      context->CreateOperation<mlir::arithm::WideMulModOp>(context, arguments[0], arguments[1], modulus->GetRes());
  wide->GetRes()->SetType(integer_type);
  auto *versioned = context->CreateOperation<mlir::scf::IfOp>(context, 1, safe->GetResultByIndex(0),
                                                              MakeRegion(context, {wide}, wide->GetRes()),
                                                              MakeRegion(context, {site.call}, site.call->GetRes()));
  versioned->GetResultByIndex(0)->SetType(integer_type);

  auto position = std::find(site.block->operations_.begin(), site.block->operations_.end(), site.call);
  auto index = static_cast<size_t>(std::distance(site.block->operations_.begin(), position));
  site.block->operations_.erase(position);
  std::vector<mlir::Operation *> replacement = {zero,        lower,       upper, modulus, nonnegative,
                                                above_lower, below_upper, range, safe,    versioned};
  site.block->operations_.insert(site.block->operations_.begin() + static_cast<std::ptrdiff_t>(index),
                                 replacement.begin(), replacement.end());
  site.call->GetRes()->ReplaceAllUsesWith(versioned->GetResultByIndex(0));
  auto *slow_yield =
      mlir::DynamicOrNull<mlir::scf::YieldOp>(versioned->GetElseRegion()->blocks_[0]->operations_.back());
  slow_yield->operands_[0] = site.call->GetRes();
  mlir::utils::RebuildFunctionUses(site.caller);
}

}  // namespace middleend::pass::transform
