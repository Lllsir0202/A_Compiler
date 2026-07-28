#include "middleend/pass/analysis/scev/scevpass.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "utils/type/type_factory.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::analysis {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}

// Follow a chain of base::CopyOp so that a copied value shares the SCEV of its source.
auto StripCopy(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr && value->op_ != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::base::CopyOp>(value->op_);
    if (copy == nullptr) {
      break;
    }
    value = copy->GetSrc();
  }
  return value;
}

auto PtrKey(const void *pointer) -> std::string { return std::to_string(reinterpret_cast<uintptr_t>(pointer)); }

// Order two SCEV nodes by their stable creation-order id. Used to canonicalize
// commutative operand lists deterministically; sorting by raw pointer address
// (the default for `const SCEV *`) varies run to run under ASLR and made the
// interned operand order -- and hence all downstream IR -- nondeterministic.
// Distinct interned nodes have distinct ids, so this keeps identical pointers
// adjacent for a following std::unique just as a pointer sort would.
auto ByStableOrder(const SCEV *lhs, const SCEV *rhs) -> bool { return lhs->GetOrder() < rhs->GetOrder(); }

auto ToString(const SCEV *scev) -> std::string {
  switch (scev->GetKind()) {
    case SCEVKind::Constant: {
      const auto *constant = static_cast<const SCEVConstant *>(scev);
      return constant->IsInt() ? std::to_string(constant->GetInt()) : std::string("f");
    }
    case SCEVKind::Unknown:
      return "%" + PtrKey(static_cast<const SCEVUnknown *>(scev)->GetValue());
    case SCEVKind::Add:
    case SCEVKind::Mul: {
      const auto *commutative = static_cast<const SCEVCommutativeExpr *>(scev);
      std::string text = "(";
      for (size_t index = 0; index < commutative->GetOperands().size(); ++index) {
        text += (index != 0 ? (scev->GetKind() == SCEVKind::Add ? " + " : " * ") : "");
        text += ToString(commutative->GetOperands()[index]);
      }
      return text + ")";
    }
    case SCEVKind::AddRec: {
      const auto *add_rec = static_cast<const SCEVAddRecExpr *>(scev);
      std::string text = "{" + ToString(add_rec->GetOperands()[0]);
      for (size_t index = 1; index < add_rec->GetOperands().size(); ++index) {
        text += ",+," + ToString(add_rec->GetOperands()[index]);
      }
      return text + "}";
    }
    case SCEVKind::SMax:
    case SCEVKind::UMin: {
      const auto *commutative = static_cast<const SCEVCommutativeExpr *>(scev);
      const std::string separator = scev->GetKind() == SCEVKind::SMax ? " smax " : " umin ";
      std::string text = "(";
      for (size_t index = 0; index < commutative->GetOperands().size(); ++index) {
        text += (index != 0 ? separator : "");
        text += ToString(commutative->GetOperands()[index]);
      }
      return text + ")";
    }
    case SCEVKind::Div: {
      const auto *div = static_cast<const SCEVDivExpr *>(scev);
      return "(" + ToString(div->GetLHS()) + " / " + ToString(div->GetRHS()) + ")";
    }
    case SCEVKind::Mod: {
      const auto *mod = static_cast<const SCEVModExpr *>(scev);
      return "(" + ToString(mod->GetLHS()) + " % " + ToString(mod->GetRHS()) + ")";
    }
    case SCEVKind::RepeatedDiv: {
      const auto *repeat = static_cast<const SCEVRepeatedDivExpr *>(scev);
      std::string exponent = std::to_string(repeat->GetBaseShift());
      if (repeat->GetStepShift() != 0) {
        exponent += "+" + std::to_string(repeat->GetStepShift()) + "*";
        exponent += repeat->IsClosed() ? ToString(repeat->GetIteration()) : "i";
      }
      return "(" + ToString(repeat->GetStart()) + " / 2^(" + exponent + "))";
    }
    case SCEVKind::ModAddRec: {
      const auto *mod_rec = static_cast<const SCEVModAddRecExpr *>(scev);
      std::string count = mod_rec->IsClosed() ? ToString(mod_rec->GetIteration()) : "i";
      return "{" + ToString(mod_rec->GetStart()) + ", +" + std::to_string(mod_rec->GetStep()) + " mod " +
             std::to_string(mod_rec->GetModulus()) + ", x" + std::to_string(mod_rec->GetFold()) + "+" +
             std::to_string(mod_rec->GetOffset()) + ", " + count + "}";
    }
  }
  return "?";
}
}  // namespace

auto SCEVPass::Run() -> void {
  PreRun();
  ClearState();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<CFGPass>("CFGPass");
  loop_info_ = GET_PASS_CONTEXT()->GetSpecPass<LoopInfoPass>("LoopInfoPass");
  if (cfg_pass == nullptr || loop_info_ == nullptr) {
    throw std::logic_error("SCEVPass requires CFGPass and LoopInfoPass");
  }

  // Map every operation to its block once; SCEV construction and invariance queries both rely on
  // knowing where a value is defined. Value pointers are unique across the module, so one global
  // map serves all functions.
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("SCEVPass only supports MLIRFunctionCFG");
    }
    for (auto *cfg_node : mlir_function_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        op_to_block_[operation] = mlir_node->GetBlock();
      }
    }
  }

  if (std::getenv("SCEV_DUMP") != nullptr) {
    for (auto *function_cfg : cfg_pass->Get()) {
      for (auto *loop : loop_info_->GetLoops(function_cfg)) {
        std::printf("[loop header=%s depth=%d] trip=%lld\n", loop->GetHeader()->Name().c_str(), loop->GetDepth(),
                    static_cast<long long>(GetConstantTripCount(loop)));
        for (auto *block : loop->GetBlocks()) {
          for (auto *operation : block->operations_) {
            if (operation->results_.size() == 1) {
              const SCEV *scev = GetSCEV(operation->results_[0]);
              if (scev->GetKind() == SCEVKind::AddRec || scev->GetKind() == SCEVKind::RepeatedDiv) {
                std::printf("    %s : %s\n", operation->Name().c_str(), ToString(scev).c_str());
              }
            }
          }
        }
      }
    }
  }
  Validate();
}

// ---- Uniquing and leaves -----------------------------------------------------------------------

auto SCEVPass::IntType() -> type::Type * {
  if (int_type_ == nullptr) {
    int_type_ = type::TypeFactory::GetIntegerType();
  }
  return int_type_;
}

auto SCEVPass::Unique(SCEV *candidate) -> const SCEV * {
  std::string key;
  switch (candidate->GetKind()) {
    case SCEVKind::Constant: {
      auto *constant = static_cast<SCEVConstant *>(candidate);
      key = "C" + (constant->IsInt() ? "i" + std::to_string(constant->GetInt())
                                     : "f" + std::to_string(std::get<float>(constant->GetValue())));
      break;
    }
    case SCEVKind::Unknown:
      key = "U" + PtrKey(static_cast<SCEVUnknown *>(candidate)->GetValue());
      break;
    case SCEVKind::Add:
    case SCEVKind::Mul:
    case SCEVKind::SMax:
    case SCEVKind::UMin: {
      auto *commutative = static_cast<SCEVCommutativeExpr *>(candidate);
      switch (candidate->GetKind()) {
        case SCEVKind::Add:
          key = "A";
          break;
        case SCEVKind::Mul:
          key = "M";
          break;
        case SCEVKind::SMax:
          key = "X";
          break;
        default:
          key = "N";
          break;
      }
      for (const auto *operand : commutative->GetOperands()) {
        key += PtrKey(operand) + ",";
      }
      break;
    }
    case SCEVKind::AddRec: {
      auto *add_rec = static_cast<SCEVAddRecExpr *>(candidate);
      key = "R" + PtrKey(add_rec->GetLoop()) + (add_rec->IsNoWrap() ? "!" : "") + ":";
      for (const auto *operand : add_rec->GetOperands()) {
        key += PtrKey(operand) + ",";
      }
      break;
    }
    case SCEVKind::Div: {
      auto *div = static_cast<SCEVDivExpr *>(candidate);
      key = "D" + PtrKey(div->GetLHS()) + ":" + PtrKey(div->GetRHS());
      break;
    }
    case SCEVKind::Mod: {
      auto *mod = static_cast<SCEVModExpr *>(candidate);
      key = "R" + PtrKey(mod->GetLHS()) + ":" + PtrKey(mod->GetRHS());
      break;
    }
    case SCEVKind::RepeatedDiv: {
      auto *repeat = static_cast<SCEVRepeatedDivExpr *>(candidate);
      key = "Q" + PtrKey(repeat->GetStart()) + ":" + PtrKey(repeat->GetIteration()) + ":" + PtrKey(repeat->GetLoop()) +
            ":" + std::to_string(repeat->GetBaseShift()) + ":" + std::to_string(repeat->GetStepShift());
      break;
    }
    case SCEVKind::ModAddRec: {
      auto *mod_rec = static_cast<SCEVModAddRecExpr *>(candidate);
      key = "M" + PtrKey(mod_rec->GetStart()) + ":" + PtrKey(mod_rec->GetIteration()) + ":" +
            PtrKey(mod_rec->GetLoop()) + ":" + std::to_string(mod_rec->GetStep()) + ":" +
            std::to_string(mod_rec->GetModulus()) + ":" + std::to_string(mod_rec->GetFold()) + ":" +
            std::to_string(mod_rec->GetOffset());
      break;
    }
  }
  auto iter = unique_map_.find(key);
  if (iter != unique_map_.end()) {
    delete candidate;
    return iter->second;
  }
  candidate->SetOrder(owned_scevs_.size());  // stable creation-order id for deterministic sorting
  unique_map_[key] = candidate;
  owned_scevs_.emplace_back(candidate);
  return candidate;
}

auto SCEVPass::GetConstant(int value) -> const SCEV * { return Unique(new SCEVConstant(value, IntType())); }

auto SCEVPass::GetUnknown(mlir::Value *value) -> const SCEV * {
  return Unique(new SCEVUnknown(value, value == nullptr ? IntType() : value->GetType()));
}

// ---- Folding builders --------------------------------------------------------------------------

auto SCEVPass::GetNegativeSCEV(const SCEV *scev) -> const SCEV * { return GetMulExpr({GetConstant(-1), scev}); }

auto SCEVPass::GetMinusSCEV(const SCEV *lhs, const SCEV *rhs) -> const SCEV * {
  return GetAddExpr({lhs, GetNegativeSCEV(rhs)});
}

auto SCEVPass::GetAddExpr(std::vector<const SCEV *> operands) -> const SCEV * {
  // Flatten nested sums so the canonical form is a single flat Add.
  std::vector<const SCEV *> flat;
  for (const auto *operand : operands) {
    if (operand->GetKind() == SCEVKind::Add) {
      const auto &inner = static_cast<const SCEVAddExpr *>(operand)->GetOperands();
      flat.insert(flat.end(), inner.begin(), inner.end());
    } else {
      flat.emplace_back(operand);
    }
  }

  // Bucket operands: fold integer constants, gather recurrences to merge per loop, and combine like
  // terms (c1*x + c2*x = (c1+c2)*x, so also x - x = 0) among everything else. The coefficient map is
  // keyed by the uniqued base pointer; term_order preserves a stable, deterministic output order.
  int constant_sum = 0;
  std::vector<const SCEV *> recurrences;
  std::vector<const SCEV *> term_order;
  std::unordered_map<const SCEV *, int> coefficients;
  for (const auto *operand : flat) {
    if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
      constant_sum += static_cast<const SCEVConstant *>(operand)->GetInt();
    } else if (operand->GetKind() == SCEVKind::AddRec) {
      recurrences.emplace_back(operand);
    } else {
      const SCEV *base = nullptr;
      int coefficient = SplitAffineTerm(operand, &base);
      if (coefficients.emplace(base, 0).second) {
        term_order.emplace_back(base);
      }
      coefficients[base] += coefficient;
    }
  }

  // Materialize the non-recurrence terms, dropping any whose coefficient cancelled to zero.
  std::vector<const SCEV *> others;
  for (const auto *base : term_order) {
    int coefficient = coefficients[base];
    if (coefficient == 0) {
      continue;
    }
    others.emplace_back(coefficient == 1 ? base : GetMulExpr({GetConstant(coefficient), base}));
  }

  // Merge recurrences that share a loop, operand-wise; a shorter chain is padded with zero steps.
  // {a0,+,a1,+,...} + {b0,+,b1,+,...} on the same loop is {a0+b0,+,a1+b1,+,...}, exact under wrap.
  std::vector<const SCEV *> merged_recurrences;
  for (const auto *recurrence : recurrences) {
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(recurrence);
    bool merged = false;
    for (auto &existing : merged_recurrences) {
      const auto *existing_rec = static_cast<const SCEVAddRecExpr *>(existing);
      if (existing_rec->GetLoop() == add_rec->GetLoop()) {
        const auto &left_ops = existing_rec->GetOperands();
        const auto &right_ops = add_rec->GetOperands();
        size_t count = std::max(left_ops.size(), right_ops.size());
        std::vector<const SCEV *> sum;
        sum.reserve(count);
        for (size_t index = 0; index < count; ++index) {
          const SCEV *left = index < left_ops.size() ? left_ops[index] : GetConstant(0);
          const SCEV *right = index < right_ops.size() ? right_ops[index] : GetConstant(0);
          sum.emplace_back(GetAddExpr({left, right}));
        }
        existing = GetAddRecExpr(sum, add_rec->GetLoop(), existing_rec->IsNoWrap() && add_rec->IsNoWrap());
        merged = true;
        break;
      }
    }
    if (!merged) {
      merged_recurrences.emplace_back(recurrence);
    }
  }

  // Recurrences that collapsed to a non-AddRec (e.g., {c,+,s} + {-c,+,-s} → 0) are moved to others.
  {
    std::vector<const SCEV *> kept;
    for (auto *candidate : merged_recurrences) {
      if (candidate->GetKind() == SCEVKind::AddRec) {
        kept.emplace_back(candidate);
      } else if (candidate->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(candidate)->IsInt()) {
        constant_sum += static_cast<const SCEVConstant *>(candidate)->GetInt();
      } else {
        others.emplace_back(candidate);
      }
    }
    merged_recurrences = std::move(kept);
  }

  // If exactly one recurrence remains and every other term is invariant in its loop, fold the
  // invariant part into the recurrence's start: {a0,+,...} + c = {a0+c,+,...}.
  if (merged_recurrences.size() == 1 && merged_recurrences[0]->GetKind() == SCEVKind::AddRec) {
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(merged_recurrences[0]);
    bool others_invariant = std::all_of(others.begin(), others.end(),
                                        [&](const SCEV *term) { return IsLoopInvariant(term, add_rec->GetLoop()); });
    if (others_invariant) {
      std::vector<const SCEV *> start_terms = others;
      start_terms.emplace_back(add_rec->GetStart());
      if (constant_sum != 0) {
        start_terms.emplace_back(GetConstant(constant_sum));
      }
      std::vector<const SCEV *> new_operands(add_rec->GetOperands().begin(), add_rec->GetOperands().end());
      new_operands[0] = GetAddExpr(start_terms);
      return GetAddRecExpr(new_operands, add_rec->GetLoop(), add_rec->IsNoWrap());
    }
  }

  // Assemble the canonical flat Add from the remaining terms.
  std::vector<const SCEV *> result = others;
  result.insert(result.end(), merged_recurrences.begin(), merged_recurrences.end());
  if (constant_sum != 0 || result.empty()) {
    result.emplace_back(GetConstant(constant_sum));
  }
  if (result.size() == 1) {
    return result[0];
  }
  std::sort(result.begin(), result.end(), ByStableOrder);
  return Unique(new SCEVAddExpr(SCEVKind::Add, IntType(), std::move(result)));
}

auto SCEVPass::GetMulExpr(std::vector<const SCEV *> operands) -> const SCEV * {
  std::vector<const SCEV *> flat;
  for (const auto *operand : operands) {
    if (operand->GetKind() == SCEVKind::Mul) {
      const auto &inner = static_cast<const SCEVMulExpr *>(operand)->GetOperands();
      flat.insert(flat.end(), inner.begin(), inner.end());
    } else {
      flat.emplace_back(operand);
    }
  }

  int constant_product = 1;
  std::vector<const SCEV *> others;
  for (const auto *operand : flat) {
    if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
      constant_product *= static_cast<const SCEVConstant *>(operand)->GetInt();
    } else {
      others.emplace_back(operand);
    }
  }
  if (constant_product == 0) {
    return GetConstant(0);
  }

  // Distribute the remaining factors into one recurrence whose loop they are all invariant in:
  // {a0,+,a1,+,...} * k = {a0*k,+,a1*k,+,...}, since the value sum_j a_j*C(i,j) scales linearly. The
  // chosen recurrence is the innermost one, so an outer induction variable (itself a recurrence of an
  // enclosing loop, hence invariant here) becomes the stride -- this is what recognizes i*j in nested
  // loops as {0,+,i}_inner. This is what turns a loop-variant multiply into an additive recurrence.
  for (size_t chosen = 0; chosen < others.size(); ++chosen) {
    if (others[chosen]->GetKind() != SCEVKind::AddRec) {
      continue;
    }
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(others[chosen]);
    std::vector<const SCEV *> factor_terms;
    bool factors_invariant = true;
    for (size_t index = 0; index < others.size(); ++index) {
      if (index == chosen) {
        continue;
      }
      if (!IsLoopInvariant(others[index], add_rec->GetLoop())) {
        factors_invariant = false;
        break;
      }
      factor_terms.emplace_back(others[index]);
    }
    if (!factors_invariant) {
      continue;
    }
    if (constant_product != 1) {
      factor_terms.emplace_back(GetConstant(constant_product));
    }
    const SCEV *factor = factor_terms.empty() ? GetConstant(1) : GetMulExpr(factor_terms);
    std::vector<const SCEV *> scaled;
    scaled.reserve(add_rec->GetOperands().size());
    for (const auto *operand : add_rec->GetOperands()) {
      scaled.emplace_back(GetMulExpr({operand, factor}));
    }
    return GetAddRecExpr(scaled, add_rec->GetLoop(), false);
  }

  std::vector<const SCEV *> result = others;
  if (constant_product != 1 || result.empty()) {
    result.emplace_back(GetConstant(constant_product));
  }
  if (result.size() == 1) {
    return result[0];
  }
  std::sort(result.begin(), result.end(), ByStableOrder);
  return Unique(new SCEVMulExpr(SCEVKind::Mul, IntType(), std::move(result)));
}

auto SCEVPass::GetAddRecExpr(const SCEV *start, const SCEV *step, Loop *loop) -> const SCEV * {
  return GetAddRecExpr(std::vector<const SCEV *>{start, step}, loop, false);
}

auto SCEVPass::GetAddRecExpr(std::vector<const SCEV *> operands, Loop *loop, bool no_wrap) -> const SCEV * {
  // Trailing zero steps carry no information: {a,+,...,+,0} collapses to the shorter chain, and a
  // recurrence that has decayed to just its start is that start.
  auto is_zero = [](const SCEV *scev) {
    return scev->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(scev)->IsInt() &&
           static_cast<const SCEVConstant *>(scev)->GetInt() == 0;
  };
  while (operands.size() > 1 && is_zero(operands.back())) {
    operands.pop_back();
  }
  if (operands.size() == 1) {
    return operands[0];
  }
  return Unique(new SCEVAddRecExpr(std::move(operands), loop, IntType(), no_wrap));
}

auto SCEVPass::GetDivExpr(const SCEV *lhs, const SCEV *rhs) -> const SCEV * {
  if (rhs->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(rhs)->IsInt()) {
    int divisor = static_cast<const SCEVConstant *>(rhs)->GetInt();
    if (divisor == 1) {
      return lhs;
    }
    // SysY integer division truncates toward zero, which is exactly C++ int division.
    if (divisor != 0 && lhs->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(lhs)->IsInt()) {
      return GetConstant(static_cast<const SCEVConstant *>(lhs)->GetInt() / divisor);
    }
    if (divisor > 1 && (divisor & (divisor - 1)) == 0 && lhs->GetKind() == SCEVKind::RepeatedDiv) {
      const auto *repeat = static_cast<const SCEVRepeatedDivExpr *>(lhs);
      return GetRepeatedDivExpr(repeat->GetStart(), repeat->GetIteration(),
                                repeat->GetBaseShift() + __builtin_ctz(static_cast<unsigned>(divisor)),
                                repeat->GetStepShift(), repeat->GetLoop());
    }
  }
  return Unique(new SCEVDivExpr(lhs, rhs, IntType()));
}

auto SCEVPass::GetModExpr(const SCEV *lhs, const SCEV *rhs) -> const SCEV * {
  if (rhs->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(rhs)->IsInt()) {
    int modulus = static_cast<const SCEVConstant *>(rhs)->GetInt();
    if (modulus == 1 || modulus == -1) {
      return GetConstant(0);
    }
    // SysY `%` truncates toward zero, which is exactly C++ int remainder.
    if (modulus != 0 && lhs->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(lhs)->IsInt()) {
      return GetConstant(static_cast<const SCEVConstant *>(lhs)->GetInt() % modulus);
    }
    // `(counter + step) % modulus` applied to a wrapped counter is one more step along the same
    // orbit, so it stays a ModAddRec with the step count bumped. This is what lets the value on the
    // back edge -- or anywhere mid-chain in an unrolled body -- be closed just like the header phi.
    const SCEV *counter = nullptr;
    int extra = 0;
    if (lhs->GetKind() == SCEVKind::Add) {
      const auto &operands = static_cast<const SCEVAddExpr *>(lhs)->GetOperands();
      if (operands.size() == 2) {
        if (GetConstantValue(operands[0], &extra)) {
          counter = operands[1];
        } else if (GetConstantValue(operands[1], &extra)) {
          counter = operands[0];
        }
      }
    }
    if (counter != nullptr && counter->GetKind() == SCEVKind::ModAddRec) {
      const auto *mod_rec = static_cast<const SCEVModAddRecExpr *>(counter);
      if (!mod_rec->IsClosed() && mod_rec->GetModulus() == modulus && mod_rec->GetStep() == extra) {
        return GetModAddRecExpr(mod_rec->GetStart(), nullptr, mod_rec->GetStep(), modulus, mod_rec->GetFold(),
                                mod_rec->GetOffset() + 1, mod_rec->GetLoop());
      }
    }
  }
  return Unique(new SCEVModExpr(lhs, rhs, IntType()));
}

auto SCEVPass::GetModAddRecExpr(const SCEV *start, const SCEV *iteration, int step, int modulus, int fold, int offset,
                                Loop *loop) -> const SCEV * {
  return Unique(new SCEVModAddRecExpr(start, iteration, step, modulus, fold, offset, loop, IntType()));
}

auto SCEVPass::GetRepeatedDivExpr(const SCEV *start, const SCEV *iteration, int base_shift, int step_shift,
                                  Loop *loop) -> const SCEV * {
  return Unique(new SCEVRepeatedDivExpr(start, iteration, base_shift, step_shift, loop, IntType()));
}

auto SCEVPass::GetSMaxExpr(std::vector<const SCEV *> operands) -> const SCEV * {
  std::vector<const SCEV *> flat;
  for (const auto *operand : operands) {
    if (operand->GetKind() == SCEVKind::SMax) {
      const auto &inner = static_cast<const SCEVSMaxExpr *>(operand)->GetOperands();
      flat.insert(flat.end(), inner.begin(), inner.end());
    } else {
      flat.emplace_back(operand);
    }
  }
  bool has_constant = false;
  int constant_max = 0;
  std::vector<const SCEV *> others;
  for (const auto *operand : flat) {
    if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
      int value = static_cast<const SCEVConstant *>(operand)->GetInt();
      constant_max = has_constant ? std::max(constant_max, value) : value;
      has_constant = true;
    } else {
      others.emplace_back(operand);
    }
  }
  std::sort(others.begin(), others.end(), ByStableOrder);
  others.erase(std::unique(others.begin(), others.end()), others.end());
  if (has_constant) {
    others.emplace_back(GetConstant(constant_max));
  }
  if (others.empty()) {
    return GetConstant(0);
  }
  if (others.size() == 1) {
    return others[0];
  }
  std::sort(others.begin(), others.end(), ByStableOrder);
  return Unique(new SCEVSMaxExpr(SCEVKind::SMax, IntType(), std::move(others)));
}

auto SCEVPass::GetUMinExpr(std::vector<const SCEV *> operands) -> const SCEV * {
  std::vector<const SCEV *> flat;
  for (const auto *operand : operands) {
    if (operand->GetKind() == SCEVKind::UMin) {
      const auto &inner = static_cast<const SCEVUMinExpr *>(operand)->GetOperands();
      flat.insert(flat.end(), inner.begin(), inner.end());
    } else {
      flat.emplace_back(operand);
    }
  }
  bool has_constant = false;
  uint32_t constant_min = 0;
  std::vector<const SCEV *> others;
  for (const auto *operand : flat) {
    if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
      auto value = static_cast<uint32_t>(static_cast<const SCEVConstant *>(operand)->GetInt());
      constant_min = has_constant ? std::min(constant_min, value) : value;
      has_constant = true;
    } else {
      others.emplace_back(operand);
    }
  }
  std::sort(others.begin(), others.end(), ByStableOrder);
  others.erase(std::unique(others.begin(), others.end()), others.end());
  if (has_constant) {
    others.emplace_back(GetConstant(static_cast<int>(constant_min)));
  }
  if (others.empty()) {
    return GetConstant(0);
  }
  if (others.size() == 1) {
    return others[0];
  }
  std::sort(others.begin(), others.end(), ByStableOrder);
  return Unique(new SCEVUMinExpr(SCEVKind::UMin, IntType(), std::move(others)));
}

auto SCEVPass::SplitAffineTerm(const SCEV *scev, const SCEV **base) -> int {
  // A product with a single integer constant factor splits into that coefficient and the rest; any
  // other term has coefficient 1. Constants are handled by the caller, never reaching here.
  if (scev->GetKind() == SCEVKind::Mul) {
    const auto &operands = static_cast<const SCEVMulExpr *>(scev)->GetOperands();
    int coefficient = 1;
    std::vector<const SCEV *> rest;
    for (const auto *operand : operands) {
      if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
        coefficient *= static_cast<const SCEVConstant *>(operand)->GetInt();
      } else {
        rest.emplace_back(operand);
      }
    }
    if (coefficient != 1 && !rest.empty()) {
      *base = rest.size() == 1 ? rest[0] : GetMulExpr(rest);
      return coefficient;
    }
  }
  *base = scev;
  return 1;
}

auto SCEVPass::GetBinomialCoefficient(const SCEV *iteration, int order) -> const SCEV * {
  // C(iteration, order) = iteration*(iteration-1)*...*(iteration-order+1) / order!. The product of
  // `order` consecutive integers is always divisible by order!, so this is exact.
  if (order == 0) {
    return GetConstant(1);
  }
  std::vector<const SCEV *> factors;
  factors.reserve(order);
  for (int index = 0; index < order; ++index) {
    factors.emplace_back(GetMinusSCEV(iteration, GetConstant(index)));
  }
  const SCEV *numerator = GetMulExpr(factors);
  int64_t factorial = 1;
  for (int index = 2; index <= order; ++index) {
    factorial *= index;
  }
  return GetDivExpr(numerator, GetConstant(static_cast<int>(factorial)));
}

auto SCEVPass::EvaluateAtIteration(const SCEV *scev, const SCEV *iteration) -> const SCEV * {
  if (scev->GetKind() == SCEVKind::RepeatedDiv) {
    const auto *repeat = static_cast<const SCEVRepeatedDivExpr *>(scev);
    if (!repeat->IsClosed()) {
      return GetRepeatedDivExpr(repeat->GetStart(), iteration, repeat->GetBaseShift(), repeat->GetStepShift(),
                                repeat->GetLoop());
    }
    return scev;
  }
  if (scev->GetKind() == SCEVKind::ModAddRec) {
    // Closing a wrapped counter is not an algebraic substitution: truncated `%` is not a
    // homomorphism on negatives and `start + step*n` overflows. Record the iteration count and let
    // the expander emit the orbit-reduction sequence documented on SCEVModAddRecExpr.
    const auto *mod_rec = static_cast<const SCEVModAddRecExpr *>(scev);
    if (!mod_rec->IsClosed()) {
      return GetModAddRecExpr(mod_rec->GetStart(), iteration, mod_rec->GetStep(), mod_rec->GetModulus(),
                              mod_rec->GetFold(), mod_rec->GetOffset(), mod_rec->GetLoop());
    }
    return scev;
  }
  // A value invariant in the recurrence's loop is unchanged by iterating; only recurrence nodes close.
  if (scev->GetKind() != SCEVKind::AddRec) {
    return scev;
  }
  const auto &operands = static_cast<const SCEVAddRecExpr *>(scev)->GetOperands();
  std::vector<const SCEV *> terms;
  terms.reserve(operands.size());
  for (size_t order = 0; order < operands.size(); ++order) {
    terms.emplace_back(GetMulExpr({operands[order], GetBinomialCoefficient(iteration, static_cast<int>(order))}));
  }
  return GetAddExpr(terms);
}

// ---- Loop invariance ---------------------------------------------------------------------------

auto SCEVPass::DefinedInLoop(mlir::Value *value, Loop *loop) -> bool {
  if (value == nullptr || value->op_ == nullptr) {
    return false;
  }
  auto iter = op_to_block_.find(value->op_);
  if (iter == op_to_block_.end()) {
    return false;
  }
  return loop->Contains(iter->second);
}

auto SCEVPass::IsLoopInvariant(const SCEV *scev, Loop *loop) -> bool {
  switch (scev->GetKind()) {
    case SCEVKind::Constant:
      return true;
    case SCEVKind::Unknown:
      return !DefinedInLoop(static_cast<const SCEVUnknown *>(scev)->GetValue(), loop);
    case SCEVKind::Add:
    case SCEVKind::Mul:
    case SCEVKind::SMax:
    case SCEVKind::UMin: {
      const auto &operands = static_cast<const SCEVCommutativeExpr *>(scev)->GetOperands();
      return std::all_of(operands.begin(), operands.end(),
                         [&](const SCEV *operand) { return IsLoopInvariant(operand, loop); });
    }
    case SCEVKind::Div: {
      const auto *div = static_cast<const SCEVDivExpr *>(scev);
      return IsLoopInvariant(div->GetLHS(), loop) && IsLoopInvariant(div->GetRHS(), loop);
    }
    case SCEVKind::Mod: {
      const auto *mod = static_cast<const SCEVModExpr *>(scev);
      return IsLoopInvariant(mod->GetLHS(), loop) && IsLoopInvariant(mod->GetRHS(), loop);
    }
    case SCEVKind::AddRec: {
      const auto *add_rec = static_cast<const SCEVAddRecExpr *>(scev);
      // A recurrence of loop L is invariant in `loop` only when L strictly encloses `loop`; then the
      // outer induction value is fixed throughout each iteration of `loop`.
      if (add_rec->GetLoop() == loop || !add_rec->GetLoop()->Contains(loop->GetHeader())) {
        return false;
      }
      const auto &operands = add_rec->GetOperands();
      return std::all_of(operands.begin(), operands.end(),
                         [&](const SCEV *operand) { return IsLoopInvariant(operand, loop); });
    }
    case SCEVKind::RepeatedDiv: {
      const auto *repeat = static_cast<const SCEVRepeatedDivExpr *>(scev);
      if (!repeat->IsClosed() && (repeat->GetLoop() == loop || !repeat->GetLoop()->Contains(loop->GetHeader()))) {
        return false;
      }
      return IsLoopInvariant(repeat->GetStart(), loop) &&
             (repeat->GetIteration() == nullptr || IsLoopInvariant(repeat->GetIteration(), loop));
    }
    case SCEVKind::ModAddRec: {
      // Like RepeatedDiv: the un-closed recurrence varies in its own loop; the closed form is
      // invariant exactly when its start and iteration count are.
      const auto *mod_rec = static_cast<const SCEVModAddRecExpr *>(scev);
      if (!mod_rec->IsClosed() && (mod_rec->GetLoop() == loop || !mod_rec->GetLoop()->Contains(loop->GetHeader()))) {
        return false;
      }
      return IsLoopInvariant(mod_rec->GetStart(), loop) &&
             (!mod_rec->IsClosed() || IsLoopInvariant(mod_rec->GetIteration(), loop));
    }
  }
  return false;
}

// ---- Construction ------------------------------------------------------------------------------

auto SCEVPass::GetBlockOf(mlir::Value *value) -> mlir::Block * {
  if (value == nullptr || value->op_ == nullptr) {
    return nullptr;
  }
  auto iter = op_to_block_.find(value->op_);
  return iter == op_to_block_.end() ? nullptr : iter->second;
}

auto SCEVPass::GetLoopFor(mlir::Block *block) -> Loop * {
  return block == nullptr ? nullptr : loop_info_->GetLoopFor(block);
}

auto SCEVPass::GetSCEV(mlir::Value *value) -> const SCEV * {
  value = StripCopy(value);
  if (value == nullptr) {
    return GetUnknown(nullptr);
  }
  auto iter = value_to_scev_.find(value);
  if (iter != value_to_scev_.end()) {
    return iter->second;
  }

  // Only integer values get rich expressions; anything else (float, pointer, bool) stays opaque so
  // that no fold or trip-count reasoning depends on it.
  if (value->op_ == nullptr || value->GetType() == nullptr || value->GetType()->GetKind() != type::TypeKind::INT) {
    const SCEV *unknown = GetUnknown(value);
    value_to_scev_[value] = unknown;
    return unknown;
  }

  // Seed a self-symbol before recursing so a self-referential header phi terminates: while building
  // the recurrence, a reference back to this value resolves to the symbol rather than recursing.
  const SCEV *self_symbol = GetUnknown(value);
  value_to_scev_[value] = self_symbol;
  const SCEV *result = CreateSCEVForOperation(value->op_, value);
  value_to_scev_[value] = result;
  return result;
}

auto SCEVPass::CreateSCEVForOperation(mlir::Operation *op, mlir::Value *result) -> const SCEV * {
  if (auto *add = mlir::DynamicOrNull<mlir::base::AddOp>(op)) {
    return GetAddExpr({GetSCEV(add->GetLHS()), GetSCEV(add->GetRHS())});
  }
  if (auto *sub = mlir::DynamicOrNull<mlir::base::SubOp>(op)) {
    return GetMinusSCEV(GetSCEV(sub->GetLHS()), GetSCEV(sub->GetRHS()));
  }
  if (auto *mul = mlir::DynamicOrNull<mlir::base::MulOp>(op)) {
    return GetMulExpr({GetSCEV(mul->GetLHS()), GetSCEV(mul->GetRHS())});
  }
  if (auto *neg = mlir::DynamicOrNull<mlir::base::NegOp>(op)) {
    return GetNegativeSCEV(GetSCEV(neg->GetOperand()));
  }
  if (auto *constant = mlir::DynamicOrNull<mlir::base::ConstIntOp>(op)) {
    return GetConstant(constant->GetValue());
  }
  if (auto *div = mlir::DynamicOrNull<mlir::base::DivOp>(op)) {
    return GetDivExpr(GetSCEV(div->GetLHS()), GetSCEV(div->GetRHS()));
  }
  if (auto *mod = mlir::DynamicOrNull<mlir::base::ModOp>(op)) {
    return GetModExpr(GetSCEV(mod->GetLHS()), GetSCEV(mod->GetRHS()));
  }
  if (auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(op)) {
    auto *block = GetBlockOf(result);
    auto *loop = GetLoopFor(block);
    // A recurrence only forms for a phi that sits in the header of the loop it belongs to.
    if (loop != nullptr && loop->GetHeader() == block) {
      const SCEV *add_rec = CreateAddRecFromPhi(phi, loop);
      if (add_rec != nullptr) {
        return add_rec;
      }
    }
    // Look through a trivial phi whose every incoming is the same value: control-flow guarantees it
    // is just a copy of that value. Loop rotation leaves the pre-rotation header IV as a one-input
    // phi in the latch, with the recurrence flowing through it; seeing through keeps the induction
    // variable analyzable exactly as in the unrotated loop (this is how SCEV stays shape-agnostic).
    const auto &incoming = phi->GetIncomingValue();
    if (!incoming.empty()) {
      mlir::Value *unique = incoming.front();
      bool all_same =
          std::all_of(incoming.begin(), incoming.end(), [&](mlir::Value *value) { return value == unique; });
      if (all_same && unique != result) {
        return GetSCEV(unique);
      }
    }
    return GetUnknown(result);
  }
  return GetUnknown(result);
}

auto SCEVPass::CreateAddRecFromPhi(mlir::base::PhiOp *phi, Loop *loop) -> const SCEV * {
  // Split the incoming edges into the single value entering from outside the loop (the start) and
  // the single value coming back around the latch. Anything else is not a simple recurrence.
  mlir::Value *init_value = nullptr;
  mlir::Value *back_value = nullptr;
  const auto &incoming_values = phi->GetIncomingValue();
  const auto &incoming_blocks = phi->GetIncomingBlock();
  for (size_t index = 0; index < incoming_values.size(); ++index) {
    if (loop->Contains(incoming_blocks[index])) {
      if (back_value != nullptr && back_value != incoming_values[index]) {
        return nullptr;
      }
      back_value = incoming_values[index];
    } else {
      if (init_value != nullptr && init_value != incoming_values[index]) {
        return nullptr;
      }
      init_value = incoming_values[index];
    }
  }
  if (init_value == nullptr || back_value == nullptr) {
    return nullptr;
  }

  const SCEV *self_symbol = value_to_scev_[phi->GetRes()];
  const SCEV *back_scev = GetSCEV(back_value);
  int repeated_shift = 0;
  if (MatchRepeatedPowerOfTwoDiv(back_scev, self_symbol, &repeated_shift)) {
    const SCEV *recurrence = GetRepeatedDivExpr(GetSCEV(init_value), nullptr, 0, repeated_shift, loop);
    ForgetValuesContaining(self_symbol);
    return recurrence;
  }
  int mod_step = 0;
  int mod_modulus = 0;
  int mod_fold = 0;
  if (MatchModAddRec(back_scev, self_symbol, &mod_step, &mod_modulus, &mod_fold)) {
    const SCEV *recurrence = GetModAddRecExpr(GetSCEV(init_value), nullptr, mod_step, mod_modulus, mod_fold, 0, loop);
    ForgetValuesContaining(self_symbol);
    return recurrence;
  }
  if (back_scev->GetKind() != SCEVKind::Add) {
    return nullptr;
  }

  // The back value must be exactly (self + step): the phi symbol appears once and the remaining
  // terms form a loop-invariant step.
  const auto &operands = static_cast<const SCEVAddExpr *>(back_scev)->GetOperands();
  std::vector<const SCEV *> step_terms;
  int self_count = 0;
  for (const auto *operand : operands) {
    if (operand == self_symbol) {
      ++self_count;
    } else {
      step_terms.emplace_back(operand);
    }
  }
  if (self_count != 1 || step_terms.empty()) {
    return nullptr;
  }
  const SCEV *step = GetAddExpr(step_terms);
  const SCEV *init_scev = GetSCEV(init_value);
  const SCEV *recurrence = nullptr;
  // A loop-invariant step gives a plain affine recurrence {init,+,step}.
  if (IsLoopInvariant(step, loop)) {
    recurrence = GetAddRecExpr(init_scev, step, loop);
  } else if (step->GetKind() == SCEVKind::AddRec && static_cast<const SCEVAddRecExpr *>(step)->GetLoop() == loop) {
    // A step that is itself a recurrence of this loop (a running sum of another induction variable,
    // as in `sum += i`) extends into a chain of recurrences: x_{n+1} = x_n + {s0,+,s1,+,...} makes x
    // the chain {init,+,s0,+,s1,+,...}, evaluated as sum_j operand_j * C(i, j).
    const auto &step_operands = static_cast<const SCEVAddRecExpr *>(step)->GetOperands();
    std::vector<const SCEV *> operands;
    operands.reserve(step_operands.size() + 1);
    operands.emplace_back(init_scev);
    operands.insert(operands.end(), step_operands.begin(), step_operands.end());
    recurrence = GetAddRecExpr(operands, loop, false);
  }
  if (recurrence == nullptr) {
    return nullptr;
  }
  // Resolving the phi used a self-symbol placeholder while walking the back-edge, so every value on
  // that chain (e.g. i+1, i+2 from an unrolled body) was memoized as `symbol + c` rather than as the
  // real recurrence. Drop those results so they are rebuilt against the finished recurrence; the phi
  // itself is re-established by GetSCEV right after this returns.
  ForgetValuesContaining(self_symbol);
  return recurrence;
}

auto SCEVPass::MatchRepeatedPowerOfTwoDiv(const SCEV *scev, const SCEV *symbol, int *total_shift) -> bool {
  int shift = 0;
  while (scev->GetKind() == SCEVKind::Div) {
    const auto *div = static_cast<const SCEVDivExpr *>(scev);
    if (div->GetRHS()->GetKind() != SCEVKind::Constant) {
      return false;
    }
    const auto *constant = static_cast<const SCEVConstant *>(div->GetRHS());
    if (!constant->IsInt()) {
      return false;
    }
    int divisor = constant->GetInt();
    if (divisor <= 1 || (divisor & (divisor - 1)) != 0) {
      return false;
    }
    shift += __builtin_ctz(static_cast<unsigned>(divisor));
    scev = div->GetLHS();
  }
  if (scev != symbol || shift == 0) {
    return false;
  }
  *total_shift = shift;
  return true;
}

auto SCEVPass::MatchModAddRec(const SCEV *scev, const SCEV *symbol, int *step, int *modulus, int *fold) -> bool {
  int chain = 0;
  int chain_step = 0;
  int chain_modulus = 0;
  // Peel `(inner + step) % modulus` off the outside repeatedly. An unrolled body produces one such
  // layer per copy; every layer must use the same constants for the orbit argument to hold.
  while (scev->GetKind() == SCEVKind::Mod) {
    const auto *mod = static_cast<const SCEVModExpr *>(scev);
    int layer_modulus = 0;
    if (!GetConstantValue(mod->GetRHS(), &layer_modulus) || layer_modulus <= 0) {
      return false;
    }
    if (mod->GetLHS()->GetKind() != SCEVKind::Add) {
      return false;
    }
    const auto &operands = static_cast<const SCEVAddExpr *>(mod->GetLHS())->GetOperands();
    if (operands.size() != 2) {
      return false;
    }
    // The addend must be a positive constant strictly below the modulus: a larger step could trigger
    // more than one reduction in a single application, which the bounded-orbit argument excludes.
    int layer_step = 0;
    const SCEV *inner = nullptr;
    if (GetConstantValue(operands[0], &layer_step)) {
      inner = operands[1];
    } else if (GetConstantValue(operands[1], &layer_step)) {
      inner = operands[0];
    } else {
      return false;
    }
    if (layer_step <= 0 || layer_step >= layer_modulus) {
      return false;
    }
    if (chain != 0 && (layer_step != chain_step || layer_modulus != chain_modulus)) {
      return false;
    }
    chain_step = layer_step;
    chain_modulus = layer_modulus;
    ++chain;
    scev = inner;
  }
  if (chain == 0 || scev != symbol) {
    return false;
  }
  *step = chain_step;
  *modulus = chain_modulus;
  *fold = chain;
  return true;
}

auto SCEVPass::ScevContains(const SCEV *scev, const SCEV *target) -> bool {
  if (scev == target) {
    return true;
  }
  switch (scev->GetKind()) {
    case SCEVKind::Add:
    case SCEVKind::Mul:
    case SCEVKind::SMax:
    case SCEVKind::UMin: {
      const auto &operands = static_cast<const SCEVCommutativeExpr *>(scev)->GetOperands();
      return std::any_of(operands.begin(), operands.end(),
                         [&](const SCEV *operand) { return ScevContains(operand, target); });
    }
    case SCEVKind::AddRec: {
      const auto &operands = static_cast<const SCEVAddRecExpr *>(scev)->GetOperands();
      return std::any_of(operands.begin(), operands.end(),
                         [&](const SCEV *operand) { return ScevContains(operand, target); });
    }
    case SCEVKind::Div: {
      const auto *div = static_cast<const SCEVDivExpr *>(scev);
      return ScevContains(div->GetLHS(), target) || ScevContains(div->GetRHS(), target);
    }
    case SCEVKind::Mod: {
      const auto *mod = static_cast<const SCEVModExpr *>(scev);
      return ScevContains(mod->GetLHS(), target) || ScevContains(mod->GetRHS(), target);
    }
    case SCEVKind::RepeatedDiv: {
      const auto *repeat = static_cast<const SCEVRepeatedDivExpr *>(scev);
      return ScevContains(repeat->GetStart(), target) ||
             (repeat->GetIteration() != nullptr && ScevContains(repeat->GetIteration(), target));
    }
    case SCEVKind::ModAddRec: {
      const auto *mod_rec = static_cast<const SCEVModAddRecExpr *>(scev);
      return ScevContains(mod_rec->GetStart(), target) ||
             (mod_rec->IsClosed() && ScevContains(mod_rec->GetIteration(), target));
    }
    default:
      return false;
  }
}

auto SCEVPass::ForgetValuesContaining(const SCEV *symbol) -> void {
  for (auto iter = value_to_scev_.begin(); iter != value_to_scev_.end();) {
    iter = ScevContains(iter->second, symbol) ? value_to_scev_.erase(iter) : std::next(iter);
  }
}

// ---- Backedge-taken / trip count ---------------------------------------------------------------

auto SCEVPass::GetConstantValue(const SCEV *scev, int *out) -> bool {
  if (scev->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(scev)->IsInt()) {
    *out = static_cast<const SCEVConstant *>(scev)->GetInt();
    return true;
  }
  return false;
}

auto SCEVPass::GetBackedgeTakenCount(Loop *loop) -> const SCEV * {
  auto iter = backedge_taken_.find(loop);
  if (iter != backedge_taken_.end()) {
    return iter->second;
  }
  const SCEV *count = ComputeBackedgeTakenCount(loop);
  backedge_taken_[loop] = count;
  return count;
}

auto SCEVPass::ComputeBackedgeTakenCount(Loop *loop) -> const SCEV * {
  // Establish the loop's recurrences before looking at its exits. Resolving a header phi seeds a
  // self-symbol placeholder while it walks the back-edge, so whichever value is queried first wins:
  // if an exit test reads a latch value (a bottom-tested loop, e.g. after rotation, tests the
  // incremented IV) then that value gets memoized as a placeholder, and the header phi later reads
  // the placeholder back and fails to form its recurrence. Asking for the header phis up front
  // makes the result independent of which value the exit test happens to name.
  for (auto *operation : loop->GetHeader()->operations_) {
    auto *phi = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
    if (phi == nullptr) {
      break;  // phis lead the block
    }
    GetSCEV(phi->GetRes());
  }

  // The loop's trip count is the minimum over every analyzable exit of that exit's own trip count:
  // whichever exit fires first stops the loop. The backedge is then taken (trip - 1) times.
  const SCEV *trip = nullptr;
  for (auto *block : loop->GetBlocks()) {
    const SCEV *exit_trip = ComputeExitCount(loop, block);
    if (exit_trip == nullptr) {
      continue;
    }
    trip = (trip == nullptr) ? exit_trip : GetUMinExpr({trip, exit_trip});
  }
  if (trip == nullptr) {
    return GetUnknown(nullptr);
  }
  return GetMinusSCEV(trip, GetConstant(1));
}

auto SCEVPass::ComputeExitCount(Loop *loop, mlir::Block *exiting) -> const SCEV * {
  // An exiting block ends in a conditional branch that leaves the loop on exactly one edge.
  if (exiting->operations_.empty()) {
    return nullptr;
  }
  auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(exiting->operations_.back());
  if (cond_br == nullptr) {
    return nullptr;
  }
  bool true_in = loop->Contains(cond_br->GetTrueTarget());
  bool false_in = loop->Contains(cond_br->GetFalseTarget());
  if (true_in == false_in) {
    return nullptr;  // not an exit test (both inside, or both outside)
  }

  mlir::Value *condition = StripCopy(cond_br->GetCondition());
  if (condition == nullptr || condition->op_ == nullptr) {
    return nullptr;
  }
  auto *compare = condition->op_;

  // Recover the predicate as "continue the loop while (lhs pred rhs)". When the loop is entered on
  // the false edge, the branch condition is negated.
  enum class Predicate { LT, LE, GT, GE, NE, EQ, NONE };
  Predicate predicate = Predicate::NONE;
  mlir::Value *lhs_value = nullptr;
  mlir::Value *rhs_value = nullptr;
  if (auto *less = mlir::DynamicOrNull<mlir::base::LessOp>(compare)) {
    predicate = Predicate::LT;
    lhs_value = less->GetLHS();
    rhs_value = less->GetRHS();
  } else if (auto *le = mlir::DynamicOrNull<mlir::base::LessEqualOp>(compare)) {
    predicate = Predicate::LE;
    lhs_value = le->GetLHS();
    rhs_value = le->GetRHS();
  } else if (auto *gt = mlir::DynamicOrNull<mlir::base::GreaterOp>(compare)) {
    predicate = Predicate::GT;
    lhs_value = gt->GetLHS();
    rhs_value = gt->GetRHS();
  } else if (auto *ge = mlir::DynamicOrNull<mlir::base::GreaterEqualOp>(compare)) {
    predicate = Predicate::GE;
    lhs_value = ge->GetLHS();
    rhs_value = ge->GetRHS();
  } else if (auto *ne = mlir::DynamicOrNull<mlir::base::NotEqualOp>(compare)) {
    predicate = Predicate::NE;
    lhs_value = ne->GetLHS();
    rhs_value = ne->GetRHS();
  } else if (auto *eq = mlir::DynamicOrNull<mlir::base::EqualOp>(compare)) {
    predicate = Predicate::EQ;
    lhs_value = eq->GetLHS();
    rhs_value = eq->GetRHS();
  } else {
    return nullptr;
  }

  const SCEV *lhs_scev = GetSCEV(lhs_value);
  const SCEV *rhs_scev = GetSCEV(rhs_value);

  // Orient so the induction variable is on the left; swap the predicate if it is on the right.
  auto swap_predicate = [](Predicate value) {
    switch (value) {
      case Predicate::LT:
        return Predicate::GT;
      case Predicate::LE:
        return Predicate::GE;
      case Predicate::GT:
        return Predicate::LT;
      case Predicate::GE:
        return Predicate::LE;
      default:
        return value;  // NE/EQ are symmetric
    }
  };
  const SCEVAddRecExpr *iv = nullptr;
  const SCEV *bound = nullptr;
  if (lhs_scev->GetKind() == SCEVKind::AddRec && static_cast<const SCEVAddRecExpr *>(lhs_scev)->GetLoop() == loop &&
      IsLoopInvariant(rhs_scev, loop)) {
    iv = static_cast<const SCEVAddRecExpr *>(lhs_scev);
    bound = rhs_scev;
  } else if (rhs_scev->GetKind() == SCEVKind::AddRec &&
             static_cast<const SCEVAddRecExpr *>(rhs_scev)->GetLoop() == loop && IsLoopInvariant(lhs_scev, loop)) {
    iv = static_cast<const SCEVAddRecExpr *>(rhs_scev);
    bound = lhs_scev;
    predicate = swap_predicate(predicate);
  } else {
    return nullptr;
  }

  // Negate the predicate when the body is entered on the false edge, so it always reads as the
  // loop-continue condition.
  if (false_in) {
    switch (predicate) {
      case Predicate::LT:
        predicate = Predicate::GE;
        break;
      case Predicate::LE:
        predicate = Predicate::GT;
        break;
      case Predicate::GT:
        predicate = Predicate::LE;
        break;
      case Predicate::GE:
        predicate = Predicate::LT;
        break;
      case Predicate::NE:
        predicate = Predicate::EQ;
        break;
      case Predicate::EQ:
        predicate = Predicate::NE;
        break;
      default:
        break;
    }
  }

  // The step sign must be a known constant to choose the monotonic direction; start and bound may be
  // symbolic, yielding a symbolic trip count.
  if (!iv->IsAffine()) {
    return nullptr;
  }
  int step = 0;
  if (!GetConstantValue(iv->GetStep(), &step) || step == 0) {
    return nullptr;
  }
  const SCEV *start = iv->GetStart();

  // Trip count for "continue while (start + i*step pred bound)". SMax(0, ...) clamps the empty case,
  // and ceil((numerator)/divisor) with divisor > 0 is floor((numerator + divisor - 1)/divisor).
  // Assumes the canonical monotonic form does not wrap; this is the only no-wrap assumption used.
  auto ceil_div = [&](const SCEV *numerator, int divisor) -> const SCEV * {
    return GetDivExpr(GetAddExpr({numerator, GetConstant(divisor - 1)}), GetConstant(divisor));
  };
  auto clamp = [&](const SCEV *count) -> const SCEV * { return GetSMaxExpr({GetConstant(0), count}); };
  const SCEV *exit_count = nullptr;
  switch (predicate) {
    case Predicate::LT:
      exit_count = step > 0 ? clamp(ceil_div(GetMinusSCEV(bound, start), step)) : nullptr;
      break;
    case Predicate::LE:
      exit_count = step > 0 ? clamp(ceil_div(GetMinusSCEV(GetAddExpr({bound, GetConstant(1)}), start), step)) : nullptr;
      break;
    case Predicate::GT:
      exit_count = step < 0 ? clamp(ceil_div(GetMinusSCEV(start, bound), -step)) : nullptr;
      break;
    case Predicate::GE:
      exit_count =
          step < 0 ? clamp(ceil_div(GetMinusSCEV(start, GetAddExpr({bound, GetConstant(-1)})), -step)) : nullptr;
      break;
    case Predicate::NE: {
      // The bound is reached exactly only for a compile-time-known monotone distance; otherwise the
      // loop is not statically bounded through this exit.
      int start_value = 0;
      int limit_value = 0;
      if (!GetConstantValue(start, &start_value) || !GetConstantValue(bound, &limit_value)) {
        return nullptr;
      }
      int64_t distance = static_cast<int64_t>(limit_value) - start_value;
      if (distance == 0) {
        exit_count = GetConstant(0);
        break;
      }
      if (distance % step != 0 || (distance > 0) != (step > 0)) {
        return nullptr;
      }
      exit_count = GetConstant(static_cast<int>(distance / step));
      break;
    }
    default:
      return nullptr;
  }
  if (exit_count == nullptr) {
    return nullptr;
  }
  // For a top-tested loop, the exit test runs before the body of the iteration that would fail, so
  // the count above is the number of header executions. For a bottom-tested loop (the shape produced
  // by loop rotation), the exit test runs after the body: the count above is how many latch tests
  // continue the loop, and the final failing iteration has already executed. Account for that last
  // header execution here, keeping trip-count reasoning independent of the CFG/phi shape.
  return exiting == loop->GetHeader() ? exit_count : GetAddExpr({exit_count, GetConstant(1)});
}

auto SCEVPass::GetConstantTripCount(Loop *loop) -> int64_t {
  int backedge = 0;
  if (GetConstantValue(GetBackedgeTakenCount(loop), &backedge) && backedge >= 0) {
    return static_cast<int64_t>(backedge) + 1;
  }
  return -1;
}

// ---- Ownership ---------------------------------------------------------------------------------

auto SCEVPass::ClearState() -> void {
  for (auto *scev : owned_scevs_) {
    delete scev;
  }
  owned_scevs_.clear();
  unique_map_.clear();
  value_to_scev_.clear();
  backedge_taken_.clear();
  op_to_block_.clear();
  delete int_type_;
  int_type_ = nullptr;
}

}  // namespace middleend::pass::analysis
