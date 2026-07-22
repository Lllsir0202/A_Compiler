#include "middleend/pass/transform/recursivememo/recursivememo.h"
#include <algorithm>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
constexpr int FIRST_BOUND = 64;
constexpr int SECOND_BOUND = 256;
constexpr int CACHE_SIZE = FIRST_BOUND * SECOND_BOUND;
int memo_block_id = 930000;

auto IsInt(mlir::Value *value) -> bool {
  return value != nullptr && value->GetType() != nullptr && value->GetType()->GetKind() == type::TypeKind::INT;
}

auto AddUses(mlir::Operation *operation) -> void {
  for (size_t index = 0; index < operation->operands_.size(); ++index) {
    if (operation->operands_[index] != nullptr) {
      operation->operands_[index]->AddUse(operation, index);
    }
  }
}
}  // namespace

auto RecursiveMemoizationPass::Run() -> void {
  PreRun();
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("RecursiveMemoizationPass requires a module");
  }

  std::unordered_set<std::string> used_names;
  for (auto *operation : module->GetBlock()->operations_) {
    if (auto *function = mlir::DynamicOrNull<mlir::base::FuncDefOp>(operation)) {
      used_names.insert(function->GetName());
    } else if (auto *global = mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(operation)) {
      used_names.insert(global->GetName());
    }
  }

  bool changed = false;
  for (const auto &candidate : FindCandidates()) {
    if (Rewrite(candidate, used_names)) {
      changed = true;
      const auto &name = candidate.function->GetName();
      used_names.insert(name);
    }
  }
  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto RecursiveMemoizationPass::FindCandidates() -> std::vector<Candidate> {
  std::vector<Candidate> candidates;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::base::FuncDefOp>(operation);
    if (function == nullptr || function->GetName() == "main") {
      continue;
    }
    size_t recursive_calls = 0;
    if (IsSafeRecursiveBody(function, &recursive_calls) && HasSingleStraightLineMainCall(function)) {
      candidates.push_back({function, recursive_calls});
    }
  }
  return candidates;
}

auto RecursiveMemoizationPass::IsSafeRecursiveBody(mlir::base::FuncDefOp *function, size_t *recursive_calls) -> bool {
  auto arguments = function->GetArguments();
  if (arguments.size() != 2 || !IsInt(arguments[0]) || !IsInt(arguments[1]) ||
      function->GetReturnType()->GetKind() != type::TypeKind::INT) {
    return false;
  }

  size_t calls = 0;
  bool has_memory_read = false;
  for (auto *block : function->GetBlocks()) {
    for (auto *operation : block->operations_) {
      if (mlir::DynamicOrNull<mlir::var::VarSetOp>(operation) != nullptr) {
        return false;
      }
      if (mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation) != nullptr) {
        has_memory_read = true;
      }
      if (auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation)) {
        if (call->GetName() != function->GetName()) {
          return false;
        }
        ++calls;
      }
    }
  }
  *recursive_calls = calls;
  return has_memory_read && calls >= 2;
}

auto RecursiveMemoizationPass::HasSingleStraightLineMainCall(mlir::base::FuncDefOp *function) -> bool {
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  size_t external_calls = 0;
  bool call_is_straight_line_main = false;
  bool main_is_recursive = false;
  for (auto *operation : module->GetBlock()->operations_) {
    auto *caller = mlir::DynamicOrNull<mlir::base::FuncDefOp>(operation);
    if (caller == nullptr || caller == function) {
      continue;
    }
    for (auto *block : caller->GetBlocks()) {
      for (auto *body_operation : block->operations_) {
        auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(body_operation);
        if (call == nullptr) {
          continue;
        }
        if (caller->GetName() == "main" && call->GetName() == "main") {
          main_is_recursive = true;
        }
        if (call->GetName() != function->GetName()) {
          continue;
        }
        ++external_calls;
        bool main_returns_directly = caller->GetName() == "main" && caller->GetBlocks().size() == 1 &&
                                     block == caller->GetEntryBlock() && !block->operations_.empty() &&
                                     mlir::DynamicOrNull<mlir::func::ReturnOp>(block->operations_.back()) != nullptr;
        call_is_straight_line_main = main_returns_directly;
        auto call_arguments = call->GetOperands();
        if (call_arguments.size() == 2) {
          const int bounds[] = {FIRST_BOUND, SECOND_BOUND};
          for (size_t index = 0; index < call_arguments.size(); ++index) {
            auto *constant = call_arguments[index]->op_ == nullptr
                                 ? nullptr
                                 : mlir::DynamicOrNull<mlir::base::ConstIntOp>(call_arguments[index]->op_);
            if (constant != nullptr && (constant->GetValue() < 0 || constant->GetValue() >= bounds[index])) {
              call_is_straight_line_main = false;
            }
          }
        }
      }
    }
  }
  return external_calls == 1 && call_is_straight_line_main && !main_is_recursive;
}

auto RecursiveMemoizationPass::UniqueName(const std::string &base,
                                          const std::unordered_set<std::string> &used_names) const -> std::string {
  if (used_names.count(base) == 0) {
    return base;
  }
  for (size_t suffix = 1;; ++suffix) {
    auto candidate = base + "_" + std::to_string(suffix);
    if (used_names.count(candidate) == 0) {
      return candidate;
    }
  }
}

auto RecursiveMemoizationPass::MakeGlobalArray(const std::string &name) -> mlir::var::GlobalVarDefOp * {
  auto *global = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::GlobalVarDefOp>(
      GET_MLIR_CONTEXT(), name, std::vector<int>{CACHE_SIZE}, std::vector<int>{}, false);
  global->GetRes()->SetType(GET_TYPE_CONTEXT()->GetArrayType(GET_TYPE_CONTEXT()->GetIntegerType(), CACHE_SIZE));
  return global;
}

auto RecursiveMemoizationPass::Rewrite(const Candidate &candidate, const std::unordered_set<std::string> &used_names)
    -> bool {
  auto *module_block = GET_MLIR_CONTEXT()->GetModuleOp()->GetBlock();
  auto *original = candidate.function;
  auto public_name = original->GetName();
  auto impl_name = UniqueName("__memo_impl_" + public_name, used_names);
  auto values_name = UniqueName("__memo_values_" + public_name, used_names);
  auto valid_name = UniqueName("__memo_valid_" + public_name, used_names);

  auto *cache_values = MakeGlobalArray(values_name);
  auto *cache_valid = MakeGlobalArray(valid_name);
  original->SetName(impl_name);
  auto *wrapper = MakeWrapper(public_name, impl_name, original, cache_values->GetRes(), cache_valid->GetRes());

  auto *lower_context = GET_MLIR_CONTEXT()->GetLowerContext();
  lower_context->MapFuncNameToFuncDefOp(impl_name, original);
  lower_context->MapFuncNameToFuncDefOp(public_name, wrapper);

  std::vector<mlir::Operation *> operations;
  operations.reserve(module_block->operations_.size() + 3);
  for (auto *operation : module_block->operations_) {
    if (operation == original) {
      operations.emplace_back(cache_values);
      operations.emplace_back(cache_valid);
      operations.emplace_back(wrapper);
    }
    operations.emplace_back(operation);
  }
  module_block->operations_ = std::move(operations);
  return true;
}

auto RecursiveMemoizationPass::MakeWrapper(const std::string &public_name, const std::string &impl_name,
                                           mlir::base::FuncDefOp *original, mlir::Value *cache_values,
                                           mlir::Value *cache_valid) -> mlir::base::FuncDefOp * {
  auto *int_type = GET_TYPE_CONTEXT()->GetIntegerType();
  auto *bool_type = GET_TYPE_CONTEXT()->GetBooleanType();
  auto *ptr_type = GET_TYPE_CONTEXT()->GetPtrType(int_type);
  auto *unknown_type = GET_TYPE_CONTEXT()->GetUnknownType();

  std::vector<mlir::Value *> arguments;
  for (auto *original_argument : original->GetArguments()) {
    auto *argument = GET_MLIR_CONTEXT()->CreateValue();
    argument->SetType(original_argument->GetType());
    GET_MLIR_CONTEXT()->GetResourceContext()->SetArgValueName(argument);
    arguments.emplace_back(argument);
  }
  auto *function_result = GET_MLIR_CONTEXT()->CreateValue();
  function_result->SetType(int_type);
  auto *wrapper = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::FuncDefOp>(function_result, public_name, arguments);

  auto *entry = wrapper->GetEntryBlock();
  auto *first_upper = new mlir::Block();
  auto *second_lower = new mlir::Block();
  auto *second_upper = new mlir::Block();
  auto *lookup = new mlir::Block();
  auto *hit = new mlir::Block();
  auto *compute = new mlir::Block();
  auto *out_of_bounds = new mlir::Block();
  auto *exit = new mlir::Block();
  NameBlock(entry, public_name + "_memo_entry");
  NameBlock(first_upper, public_name + "_memo_first_upper");
  NameBlock(second_lower, public_name + "_memo_second_lower");
  NameBlock(second_upper, public_name + "_memo_second_upper");
  NameBlock(lookup, public_name + "_memo_lookup");
  NameBlock(hit, public_name + "_memo_hit");
  NameBlock(compute, public_name + "_memo_compute");
  NameBlock(out_of_bounds, public_name + "_memo_oob");
  NameBlock(exit, public_name + "_memo_exit");

  auto make_const = [&](mlir::Block *block, int value) -> mlir::Value * {
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(int_type);
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, value);
    block->AddOperation(operation);
    return result;
  };
  auto make_compare = [&](mlir::Block *block, auto tag, mlir::Value *lhs, mlir::Value *rhs) -> mlir::Value * {
    using Op = std::remove_pointer_t<decltype(tag)>;
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(bool_type);
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<Op>(result, lhs, rhs);
    AddUses(operation);
    block->AddOperation(operation);
    return result;
  };
  auto make_binary = [&](mlir::Block *block, auto tag, mlir::Value *lhs, mlir::Value *rhs) -> mlir::Value * {
    using Op = std::remove_pointer_t<decltype(tag)>;
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(int_type);
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<Op>(result, lhs, rhs);
    AddUses(operation);
    block->AddOperation(operation);
    return result;
  };
  auto make_cond_branch = [&](mlir::Block *block, mlir::Value *condition, mlir::Block *true_block,
                              mlir::Block *false_block) {
    auto *branch = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(condition, true_block, false_block);
    AddUses(branch);
    block->AddOperation(branch);
  };
  auto make_getptr = [&](mlir::Block *block, mlir::Value *base, mlir::Value *index) -> mlir::Value * {
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), base,
                                                                                  std::vector<mlir::Value *>{index});
    operation->GetRes()->SetType(ptr_type);
    AddUses(operation);
    block->AddOperation(operation);
    return operation->GetRes();
  };
  auto make_load = [&](mlir::Block *block, mlir::Value *pointer) -> mlir::Value * {
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), pointer);
    operation->GetRes()->SetType(int_type);
    AddUses(operation);
    block->AddOperation(operation);
    return operation->GetRes();
  };
  auto make_store = [&](mlir::Block *block, mlir::Value *pointer, mlir::Value *value) {
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), pointer, value);
    operation->GetRes()->SetType(unknown_type);
    AddUses(operation);
    block->AddOperation(operation);
  };
  auto make_call = [&](mlir::Block *block) -> mlir::Value * {
    auto *result = GET_MLIR_CONTEXT()->CreateValue();
    result->SetType(int_type);
    auto *operation = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::FuncCallOp>(result, impl_name, arguments);
    AddUses(operation);
    block->AddOperation(operation);
    return result;
  };
  auto make_branch = [&](mlir::Block *block, mlir::Block *target) {
    block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(target));
  };

  auto *zero = make_const(entry, 0);
  auto *first_nonnegative = make_compare(entry, static_cast<mlir::base::GreaterEqualOp *>(nullptr), arguments[0], zero);
  make_cond_branch(entry, first_nonnegative, first_upper, out_of_bounds);

  auto *first_bound = make_const(first_upper, FIRST_BOUND);
  auto *first_in_range =
      make_compare(first_upper, static_cast<mlir::base::LessOp *>(nullptr), arguments[0], first_bound);
  make_cond_branch(first_upper, first_in_range, second_lower, out_of_bounds);

  auto *second_nonnegative =
      make_compare(second_lower, static_cast<mlir::base::GreaterEqualOp *>(nullptr), arguments[1], zero);
  make_cond_branch(second_lower, second_nonnegative, second_upper, out_of_bounds);

  auto *second_bound = make_const(second_upper, SECOND_BOUND);
  auto *second_in_range =
      make_compare(second_upper, static_cast<mlir::base::LessOp *>(nullptr), arguments[1], second_bound);
  make_cond_branch(second_upper, second_in_range, lookup, out_of_bounds);

  auto *stride = make_const(lookup, SECOND_BOUND);
  auto *row_offset = make_binary(lookup, static_cast<mlir::base::MulOp *>(nullptr), arguments[0], stride);
  auto *index = make_binary(lookup, static_cast<mlir::base::AddOp *>(nullptr), row_offset, arguments[1]);
  auto *valid_pointer = make_getptr(lookup, cache_valid, index);
  auto *valid = make_load(lookup, valid_pointer);
  auto *is_valid = make_compare(lookup, static_cast<mlir::base::NotEqualOp *>(nullptr), valid, zero);
  make_cond_branch(lookup, is_valid, hit, compute);

  auto *cached_pointer = make_getptr(hit, cache_values, index);
  auto *cached_value = make_load(hit, cached_pointer);
  make_branch(hit, exit);

  auto *computed_value = make_call(compute);
  auto *value_pointer = make_getptr(compute, cache_values, index);
  make_store(compute, value_pointer, computed_value);
  auto *one = make_const(compute, 1);
  make_store(compute, valid_pointer, one);
  make_branch(compute, exit);

  auto *uncached_value = make_call(out_of_bounds);
  make_branch(out_of_bounds, exit);

  auto *return_value = GET_MLIR_CONTEXT()->CreateValue();
  return_value->SetType(int_type);
  auto *phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(
      return_value, std::vector<mlir::Value *>{cached_value, computed_value, uncached_value},
      std::vector<mlir::Block *>{hit, compute, out_of_bounds});
  AddUses(phi);
  exit->AddOperation(phi);
  auto *return_operation =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), int_type, return_value);
  AddUses(return_operation);
  exit->AddOperation(return_operation);

  wrapper->SetBlocks({entry, first_upper, second_lower, second_upper, lookup, hit, compute, out_of_bounds, exit});
  return wrapper;
}

auto RecursiveMemoizationPass::NameBlock(mlir::Block *block, const std::string &name) -> void {
  block->block_name_ = name;
  block->block_id_ = memo_block_id++;
}

}  // namespace middleend::pass::transform
