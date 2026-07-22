#include "middleend/pass/transform/reassociate/reassociate.h"
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
struct PairKey {
  mlir::Value *first{nullptr};
  mlir::Value *second{nullptr};
  auto operator==(const PairKey &other) const -> bool { return first == other.first && second == other.second; }
};

struct PairKeyHash {
  auto operator()(const PairKey &key) const -> size_t {
    auto lhs = std::hash<mlir::Value *>{}(key.first);
    auto rhs = std::hash<mlir::Value *>{}(key.second);
    return lhs ^ (rhs + 0x9e3779b9U + (lhs << 6U) + (lhs >> 2U));
  }
};

struct Leader {
  mlir::Value *value{nullptr};
  size_t position{0};
};

auto Key(mlir::Value *lhs, mlir::Value *rhs) -> PairKey {
  if (std::less<mlir::Value *>{}(rhs, lhs)) {
    std::swap(lhs, rhs);
  }
  return {lhs, rhs};
}

auto ConstantInt(mlir::Value *value, int *result) -> bool {
  if (value == nullptr || value->op_ == nullptr) {
    return false;
  }
  auto *constant = mlir::DynamicOrNull<mlir::base::ConstIntOp>(value->op_);
  if (constant == nullptr) {
    return false;
  }
  *result = constant->GetValue();
  return true;
}

auto IsAdd(mlir::Operation *operation) -> bool { return mlir::DynamicOrNull<mlir::base::AddOp>(operation) != nullptr; }

auto IsMul(mlir::Operation *operation) -> bool { return mlir::DynamicOrNull<mlir::base::MulOp>(operation) != nullptr; }

auto WrapAdd(int lhs, int rhs) -> int {
  return static_cast<int32_t>(static_cast<uint32_t>(lhs) + static_cast<uint32_t>(rhs));
}

auto WrapSub(int lhs, int rhs) -> int {
  return static_cast<int32_t>(static_cast<uint32_t>(lhs) - static_cast<uint32_t>(rhs));
}

auto WrapMul(int lhs, int rhs) -> int {
  return static_cast<int32_t>(static_cast<uint32_t>(lhs) * static_cast<uint32_t>(rhs));
}

auto ResetOperands(mlir::Operation *operation, std::vector<mlir::Value *> operands) -> void {
  for (auto *operand : operation->operands_) {
    if (operand != nullptr) {
      operand->RemoveUse(operation);
    }
  }
  operation->operands_ = std::move(operands);
  for (size_t index = 0; index < operation->operands_.size(); ++index) {
    operation->operands_[index]->AddUse(operation, index);
  }
}
}  // namespace

auto ReassociatePass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("ReassociatePass requires CFGPass");
  }
  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_cfg == nullptr) {
      continue;
    }
    use_count_.clear();
    for (auto *cfg_node : mlir_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node == nullptr) {
        continue;
      }
      for (auto *operation : mlir_node->GetBlock()->operations_) {
        for (auto *operand : operation->operands_) {
          if (operand != nullptr) {
            ++use_count_[operand];
          }
        }
      }
    }
    for (auto *cfg_node : mlir_cfg->GetNodes()) {
      auto *mlir_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
      if (mlir_node != nullptr) {
        changed = RunOnBlock(mlir_node->GetBlock()) || changed;
      }
    }
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto ReassociatePass::RunOnBlock(mlir::Block *block) -> bool {
  std::unordered_map<PairKey, Leader, PairKeyHash> add_leaders;
  std::unordered_map<PairKey, Leader, PairKeyHash> mul_leaders;
  std::unordered_set<mlir::Operation *> seen;
  std::vector<mlir::Operation *> rewritten;
  rewritten.reserve(block->operations_.size());
  bool changed = false;
  size_t position = 0;

  for (auto *original_operation : block->operations_) {
    mlir::Operation *operation = original_operation;

    // Factor (a*c) + (b*c) into (a+b)*c when both products are
    // single-use temporaries. Integer arithmetic is modulo 2^32, so the
    // distributive rewrite preserves the target's ADDW/MULW semantics.
    if (auto *add = mlir::DynamicOrNull<mlir::base::AddOp>(operation)) {
      auto *lhs_mul =
          add->GetLHS()->op_ == nullptr ? nullptr : mlir::DynamicOrNull<mlir::base::MulOp>(add->GetLHS()->op_);
      auto *rhs_mul =
          add->GetRHS()->op_ == nullptr ? nullptr : mlir::DynamicOrNull<mlir::base::MulOp>(add->GetRHS()->op_);
      auto only_used_by_add = [&](mlir::Value *value) { return use_count_[value] == 1; };
      if (lhs_mul != nullptr && rhs_mul != nullptr && seen.count(lhs_mul) > 0 && seen.count(rhs_mul) > 0 &&
          only_used_by_add(add->GetLHS()) && only_used_by_add(add->GetRHS())) {
        mlir::Value *common = nullptr;
        for (auto *lhs_operand : lhs_mul->operands_) {
          for (auto *rhs_operand : rhs_mul->operands_) {
            if (lhs_operand == rhs_operand) {
              common = lhs_operand;
              break;
            }
          }
          if (common != nullptr) {
            break;
          }
        }
        if (common != nullptr) {
          auto other_operand = [&](mlir::base::MulOp *multiply) {
            return multiply->GetLHS() == common ? multiply->GetRHS() : multiply->GetLHS();
          };
          auto *lhs_other = other_operand(lhs_mul);
          auto *rhs_other = other_operand(rhs_mul);
          auto *sum_value = GET_MLIR_CONTEXT()->CreateValue();
          sum_value->SetType(add->GetRes()->GetType());
          auto *sum = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(sum_value, lhs_other, rhs_other);
          lhs_other->AddUse(sum, 0);
          rhs_other->AddUse(sum, 1);
          add->GetLHS()->RemoveUse(add);
          add->GetRHS()->RemoveUse(add);
          --use_count_[add->GetLHS()];
          --use_count_[add->GetRHS()];
          auto *product = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::MulOp>(add->GetRes(), sum_value, common);
          sum_value->AddUse(product, 0);
          common->AddUse(product, 1);
          rewritten.emplace_back(sum);
          seen.insert(sum);
          add_leaders.emplace(Key(lhs_other, rhs_other), Leader{sum_value, position++});
          operation = product;
          changed = true;
        }
      }
    }

    // Normalize x-C to x+(-C), so addition trees have one canonical form.
    if (auto *sub = mlir::DynamicOrNull<mlir::base::SubOp>(operation)) {
      int rhs = 0;
      if (ConstantInt(sub->GetRHS(), &rhs)) {
        auto *constant_value = GET_MLIR_CONTEXT()->CreateValue();
        constant_value->SetType(sub->GetRHS()->GetType());
        auto *constant = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(constant_value, WrapSub(0, rhs));
        rewritten.emplace_back(constant);
        auto *replacement =
            GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(sub->GetRes(), sub->GetLHS(), constant_value);
        sub->GetLHS()->RemoveUse(sub);
        sub->GetRHS()->RemoveUse(sub);
        sub->GetLHS()->AddUse(replacement, 0);
        constant_value->AddUse(replacement, 1);
        operation = replacement;
        changed = true;
      }
    }

    bool is_add = IsAdd(operation);
    bool is_mul = IsMul(operation);
    if (!is_add && !is_mul) {
      rewritten.emplace_back(operation);
      seen.insert(operation);
      ++position;
      continue;
    }

    auto &leaders = is_add ? add_leaders : mul_leaders;
    std::vector<mlir::Value *> terms;
    terms.reserve(3);
    for (auto *operand : operation->operands_) {
      bool flattened = operand != nullptr && operand->op_ != nullptr && seen.count(operand->op_) > 0 &&
                       ((is_add && IsAdd(operand->op_)) || (is_mul && IsMul(operand->op_)));
      if (flattened && terms.size() + operand->op_->operands_.size() <= 3) {
        terms.insert(terms.end(), operand->op_->operands_.begin(), operand->op_->operands_.end());
      } else {
        terms.emplace_back(operand);
      }
    }

    if (terms.size() == 3) {
      std::array<int, 3> constants{};
      std::array<bool, 3> is_constant{};
      size_t constant_count = 0;
      for (size_t index = 0; index < terms.size(); ++index) {
        is_constant[index] = ConstantInt(terms[index], &constants[index]);
        constant_count += is_constant[index] ? 1 : 0;
      }
      if (constant_count >= 2) {
        size_t first = 0;
        while (!is_constant[first]) {
          ++first;
        }
        size_t second = first + 1;
        while (second < terms.size() && !is_constant[second]) {
          ++second;
        }
        size_t remaining = 0;
        while (remaining == first || remaining == second) {
          ++remaining;
        }
        int combined =
            is_add ? WrapAdd(constants[first], constants[second]) : WrapMul(constants[first], constants[second]);
        auto *constant_value = GET_MLIR_CONTEXT()->CreateValue();
        constant_value->SetType(terms[first]->GetType());
        auto *constant = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(constant_value, combined);
        rewritten.emplace_back(constant);
        ResetOperands(operation, {terms[remaining], constant_value});
        changed = true;
      } else {
        Leader best;
        size_t remaining = 0;
        bool found = false;
        for (size_t lhs = 0; lhs < terms.size(); ++lhs) {
          for (size_t rhs = lhs + 1; rhs < terms.size(); ++rhs) {
            auto iter = leaders.find(Key(terms[lhs], terms[rhs]));
            if (iter == leaders.end() || (found && iter->second.position >= best.position)) {
              continue;
            }
            best = iter->second;
            remaining = 3 - lhs - rhs;
            found = true;
          }
        }
        if (found) {
          auto current_key = Key(operation->operands_[0], operation->operands_[1]);
          auto desired_key = Key(best.value, terms[remaining]);
          if (!(current_key == desired_key)) {
            ResetOperands(operation, {best.value, terms[remaining]});
            changed = true;
          }
        }
      }
    }

    if (operation->operands_.size() == 2) {
      auto key = Key(operation->operands_[0], operation->operands_[1]);
      if (leaders.count(key) == 0) {
        leaders.emplace(key, Leader{operation->results_[0], position});
      }
    }
    rewritten.emplace_back(operation);
    seen.insert(operation);
    ++position;
  }
  block->operations_ = std::move(rewritten);
  return changed;
}

}  // namespace middleend::pass::transform
