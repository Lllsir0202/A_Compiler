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
      return "{" + ToString(add_rec->GetStart()) + ",+," + ToString(add_rec->GetStep()) + "}";
    }
    case SCEVKind::Div: {
      const auto *div = static_cast<const SCEVDivExpr *>(scev);
      return "(" + ToString(div->GetLHS()) + " / " + ToString(div->GetRHS()) + ")";
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
              if (scev->GetKind() == SCEVKind::AddRec) {
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
    case SCEVKind::Mul: {
      auto *commutative = static_cast<SCEVCommutativeExpr *>(candidate);
      key = candidate->GetKind() == SCEVKind::Add ? "A" : "M";
      for (const auto *operand : commutative->GetOperands()) {
        key += PtrKey(operand) + ",";
      }
      break;
    }
    case SCEVKind::AddRec: {
      auto *add_rec = static_cast<SCEVAddRecExpr *>(candidate);
      key = "R" + PtrKey(add_rec->GetLoop()) + ":" + PtrKey(add_rec->GetStart()) + ":" + PtrKey(add_rec->GetStep());
      break;
    }
    case SCEVKind::Div: {
      auto *div = static_cast<SCEVDivExpr *>(candidate);
      key = "D" + PtrKey(div->GetLHS()) + ":" + PtrKey(div->GetRHS());
      break;
    }
  }
  auto iter = unique_map_.find(key);
  if (iter != unique_map_.end()) {
    delete candidate;
    return iter->second;
  }
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

  // Fold integer constants together and combine same-loop recurrences: {a1,+,s1}+{a2,+,s2} on the
  // same loop is {a1+a2,+,s1+s2}. Both folds are exact under modular arithmetic.
  int constant_sum = 0;
  std::vector<const SCEV *> recurrences;
  std::vector<const SCEV *> others;
  for (const auto *operand : flat) {
    if (operand->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(operand)->IsInt()) {
      constant_sum += static_cast<const SCEVConstant *>(operand)->GetInt();
    } else if (operand->GetKind() == SCEVKind::AddRec) {
      recurrences.emplace_back(operand);
    } else {
      others.emplace_back(operand);
    }
  }

  // Merge recurrences that share a loop.
  std::vector<const SCEV *> merged_recurrences;
  for (const auto *recurrence : recurrences) {
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(recurrence);
    bool merged = false;
    for (auto &existing : merged_recurrences) {
      const auto *existing_rec = static_cast<const SCEVAddRecExpr *>(existing);
      if (existing_rec->GetLoop() == add_rec->GetLoop()) {
        existing = GetAddRecExpr(GetAddExpr({existing_rec->GetStart(), add_rec->GetStart()}),
                                 GetAddExpr({existing_rec->GetStep(), add_rec->GetStep()}), add_rec->GetLoop());
        merged = true;
        break;
      }
    }
    if (!merged) {
      merged_recurrences.emplace_back(recurrence);
    }
  }

  // If exactly one recurrence remains and every other term is invariant in its loop, fold the
  // invariant part into the recurrence's start: {a,+,s} + c = {a+c,+,s}.
  if (merged_recurrences.size() == 1) {
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(merged_recurrences[0]);
    bool others_invariant = std::all_of(others.begin(), others.end(),
                                        [&](const SCEV *term) { return IsLoopInvariant(term, add_rec->GetLoop()); });
    if (others_invariant) {
      std::vector<const SCEV *> start_terms = others;
      start_terms.emplace_back(add_rec->GetStart());
      if (constant_sum != 0) {
        start_terms.emplace_back(GetConstant(constant_sum));
      }
      return GetAddRecExpr(GetAddExpr(start_terms), add_rec->GetStep(), add_rec->GetLoop());
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
  std::sort(result.begin(), result.end());
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

  // Distribute a constant/invariant factor into a single recurrence: {a,+,s} * k = {a*k,+,s*k}.
  // This is what turns a loop-variant multiply into an additive recurrence for strength reduction.
  std::vector<const SCEV *> recurrences;
  std::vector<const SCEV *> non_recurrences;
  for (const auto *operand : others) {
    (operand->GetKind() == SCEVKind::AddRec ? recurrences : non_recurrences).emplace_back(operand);
  }
  if (recurrences.size() == 1) {
    const auto *add_rec = static_cast<const SCEVAddRecExpr *>(recurrences[0]);
    bool factors_invariant = std::all_of(non_recurrences.begin(), non_recurrences.end(), [&](const SCEV *factor) {
      return IsLoopInvariant(factor, add_rec->GetLoop());
    });
    if (factors_invariant) {
      std::vector<const SCEV *> factor_terms = non_recurrences;
      if (constant_product != 1) {
        factor_terms.emplace_back(GetConstant(constant_product));
      }
      const SCEV *factor = factor_terms.empty() ? GetConstant(1) : GetMulExpr(factor_terms);
      return GetAddRecExpr(GetMulExpr({add_rec->GetStart(), factor}), GetMulExpr({add_rec->GetStep(), factor}),
                           add_rec->GetLoop());
    }
  }

  std::vector<const SCEV *> result = others;
  if (constant_product != 1 || result.empty()) {
    result.emplace_back(GetConstant(constant_product));
  }
  if (result.size() == 1) {
    return result[0];
  }
  std::sort(result.begin(), result.end());
  return Unique(new SCEVMulExpr(SCEVKind::Mul, IntType(), std::move(result)));
}

auto SCEVPass::GetAddRecExpr(const SCEV *start, const SCEV *step, Loop *loop) -> const SCEV * {
  // A zero step means the value never changes in the loop; it is simply its start.
  if (step->GetKind() == SCEVKind::Constant && static_cast<const SCEVConstant *>(step)->IsInt() &&
      static_cast<const SCEVConstant *>(step)->GetInt() == 0) {
    return start;
  }
  return Unique(new SCEVAddRecExpr(start, step, loop, IntType()));
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
    case SCEVKind::Mul: {
      const auto &operands = static_cast<const SCEVCommutativeExpr *>(scev)->GetOperands();
      return std::all_of(operands.begin(), operands.end(),
                         [&](const SCEV *operand) { return IsLoopInvariant(operand, loop); });
    }
    case SCEVKind::Div: {
      const auto *div = static_cast<const SCEVDivExpr *>(scev);
      return IsLoopInvariant(div->GetLHS(), loop) && IsLoopInvariant(div->GetRHS(), loop);
    }
    case SCEVKind::AddRec: {
      const auto *add_rec = static_cast<const SCEVAddRecExpr *>(scev);
      // A recurrence of loop L is invariant in `loop` only when L strictly encloses `loop`; then the
      // outer induction value is fixed throughout each iteration of `loop`.
      if (add_rec->GetLoop() == loop || !add_rec->GetLoop()->Contains(loop->GetHeader())) {
        return false;
      }
      return IsLoopInvariant(add_rec->GetStart(), loop) && IsLoopInvariant(add_rec->GetStep(), loop);
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
    return Unique(new SCEVDivExpr(GetSCEV(div->GetLHS()), GetSCEV(div->GetRHS()), IntType()));
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
  if (!IsLoopInvariant(step, loop)) {
    return nullptr;
  }
  return GetAddRecExpr(GetSCEV(init_value), step, loop);
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
  int64_t trip = GetConstantTripCount(loop);
  if (trip <= 0) {
    return GetUnknown(nullptr);
  }
  return GetConstant(static_cast<int>(trip - 1));
}

auto SCEVPass::GetConstantTripCount(Loop *loop) -> int64_t {
  // Find an exiting block: a loop block whose conditional branch leaves the loop on one edge.
  for (auto *block : loop->GetBlocks()) {
    if (block->operations_.empty()) {
      continue;
    }
    auto *cond_br = mlir::DynamicOrNull<mlir::base::CondBrOp>(block->operations_.back());
    if (cond_br == nullptr) {
      continue;
    }
    bool true_in = loop->Contains(cond_br->GetTrueTarget());
    bool false_in = loop->Contains(cond_br->GetFalseTarget());
    if (true_in == false_in) {
      continue;  // not an exit test (both inside, or both outside)
    }

    mlir::Value *condition = StripCopy(cond_br->GetCondition());
    if (condition == nullptr || condition->op_ == nullptr) {
      continue;
    }
    auto *compare = condition->op_;

    // Recover the predicate as "continue the loop while (lhs pred rhs)". When the loop is entered on
    // the false edge, the branch condition is negated.
    enum class Predicate { LT, LE, GT, GE, NE, EQ, NONE };
    Predicate predicate = Predicate::NONE;
    if (Is<mlir::base::LessOp>(compare)) {
      predicate = Predicate::LT;
    } else if (Is<mlir::base::LessEqualOp>(compare)) {
      predicate = Predicate::LE;
    } else if (Is<mlir::base::GreaterOp>(compare)) {
      predicate = Predicate::GT;
    } else if (Is<mlir::base::GreaterEqualOp>(compare)) {
      predicate = Predicate::GE;
    } else if (Is<mlir::base::NotEqualOp>(compare)) {
      predicate = Predicate::NE;
    } else if (Is<mlir::base::EqualOp>(compare)) {
      predicate = Predicate::EQ;
    } else {
      continue;
    }

    mlir::Value *lhs_value = nullptr;
    mlir::Value *rhs_value = nullptr;
    if (auto *less = mlir::DynamicOrNull<mlir::base::LessOp>(compare)) {
      lhs_value = less->GetLHS();
      rhs_value = less->GetRHS();
    } else if (auto *le = mlir::DynamicOrNull<mlir::base::LessEqualOp>(compare)) {
      lhs_value = le->GetLHS();
      rhs_value = le->GetRHS();
    } else if (auto *gt = mlir::DynamicOrNull<mlir::base::GreaterOp>(compare)) {
      lhs_value = gt->GetLHS();
      rhs_value = gt->GetRHS();
    } else if (auto *ge = mlir::DynamicOrNull<mlir::base::GreaterEqualOp>(compare)) {
      lhs_value = ge->GetLHS();
      rhs_value = ge->GetRHS();
    } else if (auto *ne = mlir::DynamicOrNull<mlir::base::NotEqualOp>(compare)) {
      lhs_value = ne->GetLHS();
      rhs_value = ne->GetRHS();
    } else if (auto *eq = mlir::DynamicOrNull<mlir::base::EqualOp>(compare)) {
      lhs_value = eq->GetLHS();
      rhs_value = eq->GetRHS();
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
      continue;
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

    int start = 0;
    int step = 0;
    int limit = 0;
    if (!GetConstantValue(iv->GetStart(), &start) || !GetConstantValue(iv->GetStep(), &step) ||
        !GetConstantValue(bound, &limit)) {
      return -1;
    }
    if (step == 0) {
      return -1;
    }

    // Body-execution count for "continue while (start + i*step pred limit)". Assumes the canonical
    // monotonic form does not wrap; this is the only place a no-wrap assumption is used.
    auto ceil_div = [](int64_t numerator, int64_t denominator) -> int64_t {
      if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
      }
      if (numerator <= 0) {
        return 0;
      }
      return (numerator + denominator - 1) / denominator;
    };
    switch (predicate) {
      case Predicate::LT:
        return step > 0 ? ceil_div(static_cast<int64_t>(limit) - start, step) : -1;
      case Predicate::LE:
        return step > 0 ? ceil_div(static_cast<int64_t>(limit) + 1 - start, step) : -1;
      case Predicate::GT:
        return step < 0 ? ceil_div(static_cast<int64_t>(start) - limit, -step) : -1;
      case Predicate::GE:
        return step < 0 ? ceil_div(static_cast<int64_t>(start) - limit + 1, -step) : -1;
      case Predicate::NE: {
        int64_t distance = static_cast<int64_t>(limit) - start;
        if (distance == 0) {
          return 0;
        }
        if (distance % step != 0 || (distance > 0) != (step > 0)) {
          return -1;  // never reaches the bound exactly: not a statically bounded loop
        }
        return distance / step;
      }
      default:
        return -1;
    }
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
