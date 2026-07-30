#include "middleend/pass/transform/mlirmatrixrowsummary/mlirmatrixrowsummary.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/arithm.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/scf.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()

namespace middleend::pass::transform {
namespace {

struct Shape {
  loopiv::Induction iv;
  mlir::Block *condition_block{nullptr};
  mlir::Block *body{nullptr};
  mlir::scf::ConditionOp *condition{nullptr};
  mlir::scf::YieldOp *yield{nullptr};
};

struct CallSite {
  mlir::func::FuncCallOp *call{nullptr};
  mlir::func::FuncDefOp *caller{nullptr};
  mlir::Block *block{nullptr};
};

auto Trace(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) break;
    value = copy->GetValue();
  }
  return value;
}

auto Same(mlir::Value *lhs, mlir::Value *rhs) -> bool { return Trace(lhs) == Trace(rhs); }

auto ConstInt(mlir::Value *value, int expected) -> bool {
  value = Trace(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  return constant != nullptr && std::holds_alternative<int>(constant->GetValue()) &&
         std::get<int>(constant->GetValue()) == expected;
}

auto ShapeOf(mlir::scf::WhileOp *loop) -> Shape {
  Shape shape;
  if (loop == nullptr || loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    return shape;
  }
  shape.condition_block = loop->GetCondRegion()->blocks_[0];
  shape.body = loop->GetBodyRegion()->blocks_[0];
  if (shape.condition_block->operations_.empty() || shape.body->operations_.empty()) return shape;
  shape.condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(shape.condition_block->operations_.back());
  shape.yield = mlir::DynamicOrNull<mlir::scf::YieldOp>(shape.body->operations_.back());
  shape.iv = loopiv::Recognize(loop);
  if (!shape.iv.valid_ || shape.iv.kind_ != loopiv::CmpKind::kLT || shape.iv.step_kind_ != loopiv::StepKind::kAdd ||
      shape.iv.step_ != 1 || !ConstInt(shape.iv.init_, 0) || shape.condition == nullptr || shape.yield == nullptr) {
    shape.iv.valid_ = false;
  }
  return shape;
}

auto DirectLoops(mlir::Block *block) -> std::vector<mlir::scf::WhileOp *> {
  std::vector<mlir::scf::WhileOp *> loops;
  for (auto *operation : block->operations_) {
    if (auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(operation)) loops.emplace_back(loop);
  }
  return loops;
}

auto Root(mlir::Value *pointer) -> mlir::Value * {
  return analysis::MLIRMemoryEffectAnalysis::GetPointerRoot(pointer).value;
}

auto Pointer(mlir::Value *pointer) -> mlir::var::VarGetPtrOp * {
  pointer = Trace(pointer);
  return pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
}

auto IndexedAs(mlir::Value *pointer, mlir::Value *root, mlir::Value *first, mlir::Value *second) -> bool {
  auto *get = Pointer(pointer);
  auto indices = get == nullptr ? std::vector<mlir::Value *>{} : get->GetIndices();
  return get != nullptr && Root(get->GetRes()) == root && indices.size() == 2 && Same(indices[0], first) &&
         Same(indices[1], second);
}

auto ContainsContinue(mlir::Operation *operation) -> bool {
  if (mlir::DynamicOrNull<mlir::scf::ContinueOp>(operation) != nullptr) return true;
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (ContainsContinue(nested)) return true;
      }
    }
  }
  return false;
}

auto SetLoopBound(mlir::scf::WhileOp *loop, mlir::Value *bound) -> bool {
  auto shape = ShapeOf(loop);
  auto *less = shape.condition == nullptr
                   ? nullptr
                   : mlir::DynamicOrNull<mlir::arithm::LessOp>(Trace(shape.condition->GetCondition())->op_);
  if (less == nullptr || !Same(less->GetLHS(), shape.condition_block->arguments_[shape.iv.index_])) {
    return false;
  }
  less->operands_[1] = bound;
  return true;
}

auto CollectCalls(mlir::func::FuncDefOp *caller, mlir::Block *block, const std::string &name,
                  std::vector<CallSite> *calls) -> void {
  for (auto *operation : block->operations_) {
    if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation);
        call != nullptr && call->GetName() == name) {
      calls->push_back({call, caller, block});
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) CollectCalls(caller, nested, name, calls);
    }
  }
}

auto AddAllOperations(mlir::Operation *operation, std::unordered_set<mlir::Operation *> *allowed) -> void {
  allowed->insert(operation);
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) AddAllOperations(nested, allowed);
    }
  }
}

auto UsesRootsOutside(mlir::Block *block, const std::unordered_set<mlir::Value *> &roots,
                      const std::unordered_set<mlir::Operation *> &allowed) -> bool {
  for (auto *operation : block->operations_) {
    if (auto *get = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(operation);
        get != nullptr && roots.count(Root(get->GetRes())) != 0 && allowed.count(operation) == 0) {
      return true;
    }
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        if (UsesRootsOutside(nested, roots, allowed)) return true;
      }
    }
  }
  return false;
}

}  // namespace

auto MLIRMatrixRowSummaryPass::Run() -> void {
  PreRun();
  kernels_ = 0;
  candidates_ = 0;
  summarized_ = 0;
  if (std::getenv("MATRIX_ROW_SUMMARY_OFF") == nullptr) {
    auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
    if (module == nullptr) throw std::logic_error("MLIR module is not set");
    for (auto *operation : module->GetBlock()->operations_) {
      auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
      if (function == nullptr) continue;
      Kernel kernel;
      if (!FindKernel(function, &kernel)) continue;
      ++kernels_;
      Candidate candidate;
      if (!FindCandidate(kernel, &candidate)) continue;
      ++candidates_;
      Rewrite(candidate);
      ++summarized_;
      break;
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr || std::getenv("MATRIX_ROW_SUMMARY_STATS") != nullptr) {
    std::cerr << "MatrixRowSummary: kernels=" << kernels_ << " candidates=" << candidates_
              << " summarized=" << summarized_ << '\n';
  }
}

auto MLIRMatrixRowSummaryPass::FindKernel(mlir::func::FuncDefOp *function, Kernel *kernel) -> bool {
  auto arguments = function->GetArguments();
  if (arguments.size() != 4 || arguments[0]->GetType()->GetKind() != type::TypeKind::INT) return false;
  auto top = DirectLoops(function->GetEntryBlock());
  if (top.size() != 2) return false;
  auto zero_row = ShapeOf(top[0]);
  auto update_k = ShapeOf(top[1]);
  if (!zero_row.iv.valid_ || !update_k.iv.valid_ || !Same(zero_row.iv.bound_, arguments[0]) ||
      !Same(update_k.iv.bound_, arguments[0])) {
    return false;
  }

  auto zero_direct = DirectLoops(zero_row.body);
  if (zero_direct.size() != 1) return false;
  auto zero_column = ShapeOf(zero_direct[0]);
  if (!zero_column.iv.valid_ || !Same(zero_column.iv.bound_, arguments[0])) return false;
  mlir::var::VarSetOp *zero_store = nullptr;
  for (auto *operation : zero_column.body->operations_) {
    if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      if (zero_store != nullptr) return false;
      zero_store = store;
    }
  }
  auto *zero_row_value = zero_row.body->arguments_[zero_row.iv.index_];
  auto *zero_column_value = zero_column.body->arguments_[zero_column.iv.index_];
  if (zero_store == nullptr || !ConstInt(zero_store->GetNewValue(), 0) ||
      !IndexedAs(zero_store->GetPtr(), arguments[3], zero_row_value, zero_column_value)) {
    return false;
  }

  auto update_i_direct = DirectLoops(update_k.body);
  if (update_i_direct.size() != 1) return false;
  auto *update_i_loop = update_i_direct[0];
  if (update_i_loop->GetCondRegion()->blocks_.size() != 1 || update_i_loop->GetBodyRegion()->blocks_.size() != 1) {
    return false;
  }
  auto *update_i_condition_block = update_i_loop->GetCondRegion()->blocks_[0];
  auto *update_i_body = update_i_loop->GetBodyRegion()->blocks_[0];
  auto *update_i_condition = mlir::DynamicOrNull<mlir::scf::ConditionOp>(update_i_condition_block->operations_.back());
  auto *update_i_less = update_i_condition == nullptr
                            ? nullptr
                            : mlir::DynamicOrNull<mlir::arithm::LessOp>(Trace(update_i_condition->GetCondition())->op_);
  if (update_i_less == nullptr || !Same(update_i_less->GetRHS(), arguments[0]) ||
      update_i_condition_block->arguments_.empty() ||
      !Same(update_i_less->GetLHS(), update_i_condition_block->arguments_[0])) {
    return false;
  }
  auto update_column_direct = DirectLoops(update_i_body);
  if (update_column_direct.size() != 1) return false;
  auto update_column = ShapeOf(update_column_direct[0]);
  if (!update_column.iv.valid_ || !Same(update_column.iv.bound_, arguments[0])) return false;

  auto *k_value = update_k.body->arguments_[update_k.iv.index_];
  auto *column_value = update_column.body->arguments_[update_column.iv.index_];
  mlir::var::VarGetValOp *a_load = nullptr;
  mlir::var::VarGetValOp *b_load = nullptr;
  mlir::var::VarGetValOp *c_load = nullptr;
  mlir::var::VarSetOp *c_store = nullptr;
  for (auto *operation : update_column.body->operations_) {
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      auto *root = Root(load->GetPtr());
      if (root == arguments[1]) a_load = load;
      if (root == arguments[2]) b_load = load;
      if (root == arguments[3]) c_load = load;
    } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
      c_store = store;
    }
  }
  if (a_load == nullptr || b_load == nullptr || c_load == nullptr || c_store == nullptr) return false;
  auto *a_pointer = Pointer(a_load->GetPtr());
  auto *b_pointer = Pointer(b_load->GetPtr());
  auto *c_pointer = Pointer(c_load->GetPtr());
  auto a_indices = a_pointer->GetIndices();
  auto b_indices = b_pointer->GetIndices();
  auto c_indices = c_pointer->GetIndices();
  if (a_indices.size() != 2 || b_indices.size() != 2 || c_indices.size() != 2 || !Same(a_indices[1], k_value) ||
      !Same(b_indices[0], k_value) || !Same(b_indices[1], column_value) || !Same(c_indices[1], column_value) ||
      !Same(a_indices[0], c_indices[0]) || !IndexedAs(c_store->GetPtr(), arguments[3], c_indices[0], column_value)) {
    return false;
  }
  auto *sum = mlir::DynamicOrNull<mlir::arithm::AddOp>(Trace(c_store->GetNewValue())->op_);
  auto *product = sum == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::MulOp>(Trace(sum->GetLHS())->op_);
  if (product == nullptr)
    product = sum == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::MulOp>(Trace(sum->GetRHS())->op_);
  if (sum == nullptr || product == nullptr ||
      !((Same(product->GetLHS(), c_load->GetRes()) && Same(product->GetRHS(), a_load->GetRes())) ||
        (Same(product->GetRHS(), c_load->GetRes()) && Same(product->GetLHS(), a_load->GetRes()))) ||
      !(Same(sum->GetLHS(), b_load->GetRes()) || Same(sum->GetRHS(), b_load->GetRes()))) {
    return false;
  }

  mlir::Value *one = nullptr;
  mlir::var::VarGetValOp *predicate_load = nullptr;
  mlir::arithm::EqualOp *predicate = nullptr;
  mlir::scf::IfOp *skip_if = nullptr;
  for (auto *operation : update_i_body->operations_) {
    if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
      auto *get = Pointer(load->GetPtr());
      auto indices = get == nullptr ? std::vector<mlir::Value *>{} : get->GetIndices();
      if (get != nullptr && Root(get->GetRes()) == arguments[1] && indices.size() == 2 &&
          Same(indices[0], update_i_body->arguments_[0]) && Same(indices[1], k_value)) {
        predicate_load = load;
      }
    }
    if (auto *equal = mlir::DynamicOrNull<mlir::arithm::EqualOp>(operation);
        equal != nullptr && predicate_load != nullptr) {
      if (Same(equal->GetLHS(), predicate_load->GetRes()) && ConstInt(equal->GetRHS(), 1)) {
        predicate = equal;
        one = Trace(equal->GetRHS());
      } else if (Same(equal->GetRHS(), predicate_load->GetRes()) && ConstInt(equal->GetLHS(), 1)) {
        predicate = equal;
        one = Trace(equal->GetLHS());
      }
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation);
        if_op != nullptr && predicate != nullptr && Same(if_op->GetCondition(), predicate->GetRes()) &&
        ContainsContinue(if_op)) {
      skip_if = if_op;
    }
  }
  if (one == nullptr || skip_if == nullptr) return false;
  kernel->function = function;
  kernel->bound = arguments[0];
  kernel->one = one;
  kernel->zero_column_loop = zero_direct[0];
  kernel->update_column_loop = update_column_direct[0];
  return true;
}

auto MLIRMatrixRowSummaryPass::FindCandidate(const Kernel &kernel, Candidate *candidate) -> bool {
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  std::vector<CallSite> calls;
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr) CollectCalls(function, function->GetEntryBlock(), kernel.function->GetName(), &calls);
  }
  if (calls.size() != 2 || calls[0].caller != calls[1].caller || calls[0].block != calls[1].block) return false;
  auto first = calls[0].call->GetOperands();
  auto second = calls[1].call->GetOperands();
  if (first.size() != 4 || second.size() != 4 || !Same(first[0], second[0])) return false;
  auto *a = Root(first[1]);
  auto *input = Root(first[2]);
  auto *scratch = Root(first[3]);
  if (a == nullptr || input == nullptr || scratch == nullptr || a == input || a == scratch || input == scratch ||
      Root(second[1]) != a || Root(second[2]) != scratch || Root(second[3]) != input) {
    return false;
  }
  auto *caller = calls[0].caller;
  auto top = DirectLoops(caller->GetEntryBlock());
  InputLoop input_loop;
  mlir::scf::WhileOp *checksum_column = nullptr;
  mlir::scf::WhileOp *input_row = nullptr;
  mlir::scf::WhileOp *checksum_row = nullptr;
  for (auto *outer_loop : top) {
    auto outer = ShapeOf(outer_loop);
    if (!outer.iv.valid_ || !Same(outer.iv.bound_, first[0])) continue;
    auto nested = DirectLoops(outer.body);
    if (nested.size() != 1) continue;
    auto inner = ShapeOf(nested[0]);
    if (!inner.iv.valid_ || !Same(inner.iv.bound_, first[0])) continue;
    auto *row = outer.body->arguments_[outer.iv.index_];
    auto *column = inner.body->arguments_[inner.iv.index_];
    mlir::func::FuncCallOp *getint = nullptr;
    mlir::var::VarSetOp *store = nullptr;
    mlir::var::VarGetValOp *load = nullptr;
    mlir::arithm::AddOp *add = nullptr;
    bool extra_input_access = false;
    for (auto *operation : inner.body->operations_) {
      if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation);
          call != nullptr && call->GetName() == "getint")
        getint = call;
      if (auto *candidate_store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation);
          candidate_store != nullptr && IndexedAs(candidate_store->GetPtr(), input, row, column)) {
        if (store != nullptr) extra_input_access = true;
        store = candidate_store;
      }
      if (auto *candidate_load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation);
          candidate_load != nullptr && IndexedAs(candidate_load->GetPtr(), input, row, column)) {
        if (load != nullptr) extra_input_access = true;
        load = candidate_load;
      }
      if (auto *candidate_add = mlir::DynamicOrNull<mlir::arithm::AddOp>(operation);
          candidate_add != nullptr && load != nullptr &&
          (Same(candidate_add->GetLHS(), load->GetRes()) || Same(candidate_add->GetRHS(), load->GetRes()))) {
        add = candidate_add;
      }
    }
    if (getint != nullptr && store != nullptr && !extra_input_access && Same(store->GetNewValue(), getint->GetRes())) {
      if (input_row != nullptr) return false;
      input_row = outer_loop;
      input_loop = {outer_loop, nested[0], getint, Pointer(store->GetPtr())};
    }
    if (load != nullptr && add != nullptr && !extra_input_access && inner.condition->GetOriginalValues().size() == 2 &&
        inner.body->arguments_.size() == 2 && inner.yield->GetOperands().size() == 2) {
      size_t accumulator_index = 1 - inner.iv.index_;
      auto *accumulator = inner.body->arguments_[accumulator_index];
      bool exact_add = (Same(add->GetLHS(), load->GetRes()) && Same(add->GetRHS(), accumulator)) ||
                       (Same(add->GetRHS(), load->GetRes()) && Same(add->GetLHS(), accumulator));
      bool exact_yield = Same(inner.yield->GetOperands()[accumulator_index], add->GetRes());
      bool observed = false;
      auto *inner_result = nested[0]->GetResultByIndex(accumulator_index);
      for (size_t result_index = 0; result_index < outer.yield->GetOperands().size(); ++result_index) {
        if (Same(outer.yield->GetOperands()[result_index], inner_result) &&
            !outer_loop->GetResultByIndex(result_index)->GetUses().empty()) {
          observed = true;
        }
      }
      if (!exact_add || !exact_yield || !observed) continue;
      if (checksum_row != nullptr) return false;
      checksum_row = outer_loop;
      checksum_column = nested[0];
    }
  }
  if (input_row == nullptr || checksum_row == nullptr || checksum_column == nullptr) return false;

  std::unordered_set<mlir::Operation *> allowed;
  AddAllOperations(input_row, &allowed);
  AddAllOperations(checksum_row, &allowed);
  for (const auto &site : calls) {
    allowed.insert(site.call);
    for (auto *operand : site.call->GetOperands()) {
      if (operand != nullptr && operand->op_ != nullptr) allowed.insert(operand->op_);
    }
  }
  if (UsesRootsOutside(caller->GetEntryBlock(), {input, scratch}, allowed)) return false;
  candidate->kernel = kernel;
  candidate->caller = caller;
  candidate->summary_root = input;
  candidate->scratch_root = scratch;
  candidate->bound = first[0];
  candidate->input = input_loop;
  candidate->checksum_column_loop = checksum_column;
  return true;
}

auto MLIRMatrixRowSummaryPass::Rewrite(const Candidate &candidate) -> void {
  SetLoopBound(candidate.kernel.zero_column_loop, candidate.kernel.one);
  SetLoopBound(candidate.kernel.update_column_loop, candidate.kernel.one);

  auto input_shape = ShapeOf(candidate.input.column_loop);
  auto *integer_type = input_shape.body->arguments_[input_shape.iv.index_]->GetType();
  // Structured lowering expects the condition/body argument at a carried
  // position to be the same Value object (the builder follows this convention).
  auto *accumulator = GET_MLIR_CONTEXT()->CreateValue();
  accumulator->SetType(integer_type);
  input_shape.condition_block->AddArgument(accumulator);
  input_shape.body->AddArgument(accumulator);
  auto carried = input_shape.condition->GetOperands();
  auto originals = input_shape.condition->GetOriginalValues();
  carried.emplace_back(accumulator);
  originals.emplace_back(input_shape.iv.init_);
  input_shape.condition->SetLoopCarriedValues(carried, originals);

  auto *next_sum = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), accumulator,
                                                                            candidate.input.input_call->GetRes());
  next_sum->GetRes()->SetType(integer_type);
  input_shape.body->operations_.insert(input_shape.body->operations_.end() - 1, next_sum);
  input_shape.yield->operands_.emplace_back(next_sum->GetRes());
  auto *sum_result = GET_MLIR_CONTEXT()->CreateValue();
  sum_result->SetType(integer_type);
  sum_result->op_ = candidate.input.column_loop;
  candidate.input.column_loop->results_.emplace_back(sum_result);

  auto row_shape = ShapeOf(candidate.input.row_loop);
  auto *row_value = row_shape.body->arguments_[row_shape.iv.index_];
  auto *summary_pointer = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
      GET_MLIR_CONTEXT(), candidate.input.store_pointer->GetPtr(),
      std::vector<mlir::Value *>{row_value, input_shape.iv.init_});
  summary_pointer->GetRes()->SetType(candidate.input.store_pointer->GetRes()->GetType());
  auto *summary_store = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(),
                                                                                 summary_pointer->GetRes(), sum_result);
  summary_store->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
  auto loop_position =
      std::find(row_shape.body->operations_.begin(), row_shape.body->operations_.end(), candidate.input.column_loop);
  row_shape.body->operations_.insert(loop_position + 1, summary_pointer);
  row_shape.body->operations_.insert(loop_position + 2, summary_store);

  // A positive row bound is guaranteed whenever the checksum row executes, so
  // a single column-zero iteration is the exact row-summary observation.
  SetLoopBound(candidate.checksum_column_loop, candidate.kernel.one);
  mlir::utils::RebuildFunctionUses(candidate.kernel.function);
  mlir::utils::RebuildFunctionUses(candidate.caller);
}

}  // namespace middleend::pass::transform
