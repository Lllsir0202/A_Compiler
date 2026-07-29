#include "middleend/pass/transform/reassociate/reassociate.h"
#include <array>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <iostream>
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
  linear_candidates_ = 0;
  linear_simplified_ = 0;
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
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "LinearReassociate: candidates=" << linear_candidates_
              << " simplified=" << linear_simplified_ << '\n';
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

    std::vector<mlir::Operation *> linear_replacement;
    if (std::getenv("REASSOCIATE_LINEAR_OFF") == nullptr &&
        Linearize(operation, seen, &linear_replacement)) {
      for (auto *replacement : linear_replacement) {
        rewritten.emplace_back(replacement);
        seen.insert(replacement);
        ++position;
      }
      changed = true;
      continue;
    }

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

auto ReassociatePass::Linearize(
    mlir::Operation *operation,
    const std::unordered_set<mlir::Operation *> &seen,
    std::vector<mlir::Operation *> *replacement) -> bool {
  auto *root_add = mlir::DynamicOrNull<mlir::base::AddOp>(operation);
  auto *root_sub = mlir::DynamicOrNull<mlir::base::SubOp>(operation);
  auto *root_neg = mlir::DynamicOrNull<mlir::base::NegOp>(operation);
  if ((root_add == nullptr && root_sub == nullptr && root_neg == nullptr) ||
      operation->results_.size() != 1 ||
      operation->results_[0]->GetType()->GetKind() != type::TypeKind::INT) {
    return false;
  }

  using Coefficient = uint32_t;
  std::unordered_map<mlir::Value *, Coefficient> coefficients;
  std::unordered_map<mlir::Value *, size_t> internal_references;
  std::unordered_set<mlir::Operation *> expanded;
  Coefficient constant = 0;
  size_t visits = 0;
  bool valid = true;

  std::function<void(mlir::Value *, Coefficient)> collect =
      [&](mlir::Value *value, Coefficient factor) {
        if (!valid || ++visits > 64) {
          valid = false;
          return;
        }
        int integer = 0;
        if (ConstantInt(value, &integer)) {
          constant += factor * static_cast<Coefficient>(integer);
          return;
        }
        auto *definition = value == nullptr ? nullptr : value->op_;
        auto *add = definition == nullptr
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::base::AddOp>(definition);
        auto *sub = definition == nullptr
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::base::SubOp>(definition);
        auto *neg = definition == nullptr
                        ? nullptr
                        : mlir::DynamicOrNull<mlir::base::NegOp>(definition);
        if (seen.count(definition) > 0 &&
            (add != nullptr || sub != nullptr || neg != nullptr)) {
          ++internal_references[value];
          expanded.insert(definition);
          if (add != nullptr) {
            collect(add->GetLHS(), factor);
            collect(add->GetRHS(), factor);
          } else if (sub != nullptr) {
            collect(sub->GetLHS(), factor);
            collect(sub->GetRHS(), Coefficient{0} - factor);
          } else {
            collect(neg->GetOperand(), Coefficient{0} - factor);
          }
          return;
        }
        coefficients[value] += factor;
      };

  expanded.insert(operation);
  if (root_add != nullptr) {
    collect(root_add->GetLHS(), 1);
    collect(root_add->GetRHS(), 1);
  } else if (root_sub != nullptr) {
    collect(root_sub->GetLHS(), 1);
    collect(root_sub->GetRHS(), Coefficient{0} - Coefficient{1});
  } else {
    collect(root_neg->GetOperand(), Coefficient{0} - Coefficient{1});
  }
  if (!valid) return false;

  for (auto *expanded_operation : expanded) {
    if (expanded_operation == operation) continue;
    auto *value = expanded_operation->results_[0];
    if (use_count_[value] != internal_references[value]) {
      return false;
    }
  }

  size_t positive_count = constant == 0 ? 0 : 1;
  size_t negative_count = 0;
  size_t multiply_count = 0;
  for (const auto &[value, coefficient] : coefficients) {
    (void)value;
    if (coefficient == 0) continue;
    if (coefficient == std::numeric_limits<Coefficient>::max()) {
      ++negative_count;
    } else {
      ++positive_count;
      if (coefficient != 1) ++multiply_count;
    }
  }
  size_t new_arithmetic = multiply_count;
  if (positive_count > 0) new_arithmetic += positive_count - 1;
  if (negative_count > 0) new_arithmetic += negative_count - 1;
  if (positive_count > 0 && negative_count > 0) {
    ++new_arithmetic;
  } else if (negative_count > 0) {
    ++new_arithmetic;
  }
  auto old_arithmetic = expanded.size();
  if (old_arithmetic <= 1) return false;
  ++linear_candidates_;
  // A one-instruction local win can lengthen a dependency chain or increase
  // pressure after regrouping. Require two eliminated arithmetic operations;
  // this keeps the transform focused on genuine cancellation trees.
  if (new_arithmetic + 2 > old_arithmetic) return false;

  auto *result = operation->results_[0];
  auto *result_type = result->GetType();
  auto make_value = [&]() {
    auto *value = GET_MLIR_CONTEXT()->CreateValue();
    value->SetType(result_type);
    return value;
  };
  auto make_constant = [&](int value, mlir::Value *target = nullptr) {
    if (target == nullptr) target = make_value();
    auto *constant_operation =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(
            target, value);
    replacement->emplace_back(constant_operation);
    return target;
  };
  auto make_binary = [&](auto operation_factory, mlir::Value *lhs,
                         mlir::Value *rhs, mlir::Value *target = nullptr) {
    if (target == nullptr) target = make_value();
    auto *created = operation_factory(target, lhs, rhs);
    lhs->AddUse(created, 0);
    rhs->AddUse(created, 1);
    ++use_count_[lhs];
    ++use_count_[rhs];
    replacement->emplace_back(created);
    return target;
  };

  std::vector<std::pair<mlir::Value *, Coefficient>> ordered;
  ordered.reserve(coefficients.size());
  for (const auto &[value, coefficient] : coefficients) {
    if (coefficient != 0) ordered.emplace_back(value, coefficient);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const auto &lhs, const auto &rhs) {
              return std::less<mlir::Value *>{}(lhs.first, rhs.first);
            });

  std::vector<mlir::Value *> positive;
  std::vector<mlir::Value *> negative;
  for (const auto &[value, coefficient] : ordered) {
    if (coefficient == 1) {
      positive.emplace_back(value);
    } else if (coefficient == std::numeric_limits<Coefficient>::max()) {
      negative.emplace_back(value);
    } else {
      auto *coefficient_value = make_constant(static_cast<int32_t>(coefficient));
      positive.emplace_back(make_binary(
          [&](mlir::Value *target, mlir::Value *lhs, mlir::Value *rhs) {
            return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::MulOp>(
                target, lhs, rhs);
          },
          value, coefficient_value));
    }
  }
  if (constant != 0) {
    positive.emplace_back(make_constant(static_cast<int32_t>(constant)));
  }

  auto build_sum = [&](std::vector<mlir::Value *> values,
                       mlir::Value *final_result) {
    auto *sum = values[0];
    for (size_t index = 1; index < values.size(); ++index) {
      auto *target =
          index + 1 == values.size() ? final_result : nullptr;
      sum = make_binary(
          [&](mlir::Value *created_result, mlir::Value *lhs,
              mlir::Value *rhs) {
            return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::AddOp>(
                created_result, lhs, rhs);
          },
          sum, values[index], target);
    }
    return sum;
  };

  for (auto *operand : operation->operands_) {
    operand->RemoveUse(operation);
    if (use_count_[operand] > 0) --use_count_[operand];
  }

  if (positive.empty() && negative.empty()) {
    make_constant(0, result);
  } else if (negative.empty()) {
    auto *sum =
        build_sum(positive, positive.size() == 1 ? nullptr : result);
    if (positive.size() == 1) {
      auto transferred = use_count_[result];
      result->ReplaceAllUsesWith(sum);
      use_count_[sum] += transferred;
      use_count_[result] = 0;
    }
  } else if (positive.empty()) {
    auto *negative_sum = build_sum(negative, nullptr);
    auto *negation =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::base::NegOp>(
            result, negative_sum);
    negative_sum->AddUse(negation, 0);
    ++use_count_[negative_sum];
    replacement->emplace_back(negation);
  } else {
    auto *positive_sum = build_sum(positive, nullptr);
    auto *negative_sum = build_sum(negative, nullptr);
    make_binary(
        [&](mlir::Value *target, mlir::Value *lhs, mlir::Value *rhs) {
          return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::SubOp>(
              target, lhs, rhs);
        },
        positive_sum, negative_sum, result);
  }

  ++linear_simplified_;
  return true;
}

}  // namespace middleend::pass::transform
