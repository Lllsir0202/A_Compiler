#include "middleend/pass/transform/sccp/sccp.h"
#include <cmath>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/basic.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/transform/canonicalization/phi.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}

auto UnknownValue() -> SCCPPass::LatticeValue { return {}; }

auto OverdefinedValue() -> SCCPPass::LatticeValue {
  SCCPPass::LatticeValue value;
  value.kind = SCCPPass::LatticeKind::OVERDEFINED;
  return value;
}

auto ConstantInt(int value) -> SCCPPass::LatticeValue {
  SCCPPass::LatticeValue lattice_value;
  lattice_value.kind = SCCPPass::LatticeKind::CONSTANT;
  lattice_value.constant.value = value;
  return lattice_value;
}

auto ConstantFloat(float value) -> SCCPPass::LatticeValue {
  SCCPPass::LatticeValue lattice_value;
  lattice_value.kind = SCCPPass::LatticeKind::CONSTANT;
  lattice_value.constant.value = value;
  return lattice_value;
}

auto ConstantBool(bool value) -> SCCPPass::LatticeValue {
  SCCPPass::LatticeValue lattice_value;
  lattice_value.kind = SCCPPass::LatticeKind::CONSTANT;
  lattice_value.constant.value = value;
  return lattice_value;
}

auto IsConstantInt(const SCCPPass::LatticeValue &value) -> bool {
  return value.kind == SCCPPass::LatticeKind::CONSTANT && std::holds_alternative<int>(value.constant.value);
}

auto IsConstantFloat(const SCCPPass::LatticeValue &value) -> bool {
  return value.kind == SCCPPass::LatticeKind::CONSTANT && std::holds_alternative<float>(value.constant.value);
}

auto IsConstantBool(const SCCPPass::LatticeValue &value) -> bool {
  return value.kind == SCCPPass::LatticeKind::CONSTANT && std::holds_alternative<bool>(value.constant.value);
}

auto GetInt(const SCCPPass::LatticeValue &value) -> int { return std::get<int>(value.constant.value); }
auto GetFloat(const SCCPPass::LatticeValue &value) -> float { return std::get<float>(value.constant.value); }
auto GetBool(const SCCPPass::LatticeValue &value) -> bool { return std::get<bool>(value.constant.value); }
}  // namespace

auto SCCPPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  if (cfg_pass == nullptr) {
    throw std::logic_error("SCCPPass requires CFGPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("SCCPPass only supports MLIRFunctionCFG");
    }
    changed = RunOnFunction(mlir_function_cfg) || changed;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
    auto *phi_canonicalization_pass =
        context::PassContext::CreatePass<PhiCanonicalizationPass>(compiler_context_, PassStage::BASEIR);
    phi_canonicalization_pass->Run();
    delete phi_canonicalization_pass;
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto SCCPPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg) -> bool {
  ResetFunctionState();
  current_function_cfg_ = function_cfg;
  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("SCCPPass only supports MLIRCFGNode");
    }
    auto *block = mlir_cfg_node->GetBlock();
    block_to_node_[block] = cfg_node;
    for (auto *operation : block->operations_) {
      operation_to_block_[operation] = block;
      for (auto *operand : operation->operands_) {
        if (operand != nullptr) {
          value_users_[operand].emplace_back(operation);
        }
      }
    }
  }

  MarkBlockExecutable(current_function_cfg_->GetEntryNode());
  while (!cfg_worklist_.empty() || !ssa_worklist_.empty()) {
    while (!cfg_worklist_.empty()) {
      auto *cfg_node = cfg_worklist_.front();
      cfg_worklist_.pop_front();
      VisitBlock(cfg_node);
    }
    while (!ssa_worklist_.empty()) {
      auto *operation = ssa_worklist_.front();
      ssa_worklist_.pop_front();
      auto *block = GetBlockOfOperation(operation);
      if (block == nullptr) {
        continue;
      }
      auto *cfg_node = block_to_node_[block];
      if (executable_blocks_.count(cfg_node) == 0) {
        continue;
      }
      VisitOperation(operation);
    }
  }

  bool changed = ReplaceConstantsAndBranches();
  ResetFunctionState();
  return changed;
}

auto SCCPPass::ResetFunctionState() -> void {
  current_function_cfg_ = nullptr;
  operation_to_block_.clear();
  value_users_.clear();
  block_to_node_.clear();
  value_states_.clear();
  executable_blocks_.clear();
  executable_edges_.clear();
  cfg_worklist_.clear();
  ssa_worklist_.clear();
}

auto SCCPPass::MarkBlockExecutable(analysis::CFGNode *cfg_node) -> void {
  if (cfg_node == nullptr || executable_blocks_.count(cfg_node) > 0) {
    return;
  }
  executable_blocks_.insert(cfg_node);
  cfg_worklist_.emplace_back(cfg_node);
}

auto SCCPPass::MarkEdgeExecutable(analysis::CFGNode *from, analysis::CFGNode *to) -> void {
  if (from == nullptr || to == nullptr) {
    return;
  }
  CFGEdge edge{from, to};
  if (executable_edges_.count(edge) > 0) {
    return;
  }
  executable_edges_.insert(edge);
  MarkBlockExecutable(to);
  auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(to);
  if (mlir_cfg_node == nullptr) {
    throw std::logic_error("SCCPPass only supports MLIRCFGNode");
  }
  for (auto *operation : mlir_cfg_node->GetBlock()->operations_) {
    if (Is<mlir::base::PhiOp>(operation)) {
      MarkOperationChanged(operation);
      continue;
    }
    break;
  }
}

auto SCCPPass::VisitBlock(analysis::CFGNode *cfg_node) -> void {
  auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
  if (mlir_cfg_node == nullptr) {
    throw std::logic_error("SCCPPass only supports MLIRCFGNode");
  }
  for (auto *operation : mlir_cfg_node->GetBlock()->operations_) {
    VisitOperation(operation);
  }
}

auto SCCPPass::VisitOperation(mlir::Operation *operation) -> void {
  auto *block = GetBlockOfOperation(operation);
  if (block == nullptr) {
    return;
  }
  auto *cfg_node = block_to_node_[block];
  if (GetTerminator(block) == operation) {
    VisitTerminator(cfg_node, operation);
  }
  if (!IsTrackedResultOperation(operation)) {
    return;
  }
  MergeInValue(operation->results_[0], EvaluateOperation(operation));
}

auto SCCPPass::VisitTerminator(analysis::CFGNode *cfg_node, mlir::Operation *terminator) -> void {
  if (auto *br_op = mlir::DynamicOrNull<mlir::base::BrOp>(terminator)) {
    MarkEdgeExecutable(cfg_node, current_function_cfg_->GetNodeByBlock(br_op->GetTarget()));
    return;
  }
  if (auto *cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator)) {
    auto condition = GetValueState(cond_br_op->GetCondition());
    auto *true_node = current_function_cfg_->GetNodeByBlock(cond_br_op->GetTrueTarget());
    auto *false_node = current_function_cfg_->GetNodeByBlock(cond_br_op->GetFalseTarget());
    if (IsConstantBool(condition)) {
      MarkEdgeExecutable(cfg_node, GetBool(condition) ? true_node : false_node);
      return;
    }
    if (condition.kind == LatticeKind::UNKNOWN) {
      return;
    }
    MarkEdgeExecutable(cfg_node, true_node);
    MarkEdgeExecutable(cfg_node, false_node);
  }
}

auto SCCPPass::ReplaceConstantsAndBranches() -> bool {
  bool changed = false;
  for (auto *cfg_node : current_function_cfg_->GetNodes()) {
    if (executable_blocks_.count(cfg_node) == 0) {
      continue;
    }
    auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node);
    if (mlir_cfg_node == nullptr) {
      throw std::logic_error("SCCPPass only supports MLIRCFGNode");
    }
    auto *block = mlir_cfg_node->GetBlock();
    for (auto *operation : block->operations_) {
      if (Is<mlir::base::PhiOp>(operation)) {
        continue;
      }
      if (IsTrackedResultOperation(operation)) {
        changed = ReplaceConstantValue(operation, GetValueState(operation->results_[0])) || changed;
      }
    }
    changed = RewriteConstantBranch(cfg_node, GetTerminator(block)) || changed;
  }
  return changed;
}

auto SCCPPass::ReplaceConstantValue(mlir::Operation *operation, const LatticeValue &lattice_value) -> bool {
  if (!IsReplaceableConstant(lattice_value)) {
    return false;
  }
  if (Is<mlir::base::ConstIntOp>(operation) || Is<mlir::base::ConstFloatOp>(operation)) {
    return false;
  }
  auto *result = operation->results_[0];
  auto *constant_op = CreateConstantOperation(result, lattice_value.constant);
  if (constant_op == nullptr) {
    return false;
  }
  mlir::Block *block = GetBlockOfOperation(operation);
  if (block == nullptr) {
    return false;
  }
  for (auto *operand : operation->operands_) {
    if (operand != nullptr) {
      operand->RemoveUse(operation);
    }
  }
  operation_to_block_.erase(operation);
  operation_to_block_[constant_op] = block;
  for (auto *&block_operation : block->operations_) {
    if (block_operation == operation) {
      block_operation = constant_op;
      return true;
    }
  }
  return false;
}

auto SCCPPass::RewriteConstantBranch(analysis::CFGNode *cfg_node, mlir::Operation *terminator) -> bool {
  auto *cond_br_op = mlir::DynamicOrNull<mlir::base::CondBrOp>(terminator);
  if (cond_br_op == nullptr) {
    return false;
  }
  auto condition = GetValueState(cond_br_op->GetCondition());
  if (!IsConstantBool(condition)) {
    return false;
  }
  auto *target_block = GetBool(condition) ? cond_br_op->GetTrueTarget() : cond_br_op->GetFalseTarget();
  auto *block = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node)->GetBlock();
  cond_br_op->GetCondition()->RemoveUse(cond_br_op);
  auto *br_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(target_block);
  block->operations_.back() = br_op;
  operation_to_block_.erase(cond_br_op);
  operation_to_block_[br_op] = block;
  return true;
}

auto SCCPPass::GetValueState(mlir::Value *value) -> LatticeValue {
  if (value == nullptr) {
    return OverdefinedValue();
  }
  auto iter = value_states_.find(value);
  if (iter != value_states_.end()) {
    return iter->second;
  }
  if (value->op_ == nullptr) {
    return OverdefinedValue();
  }
  if (!IsTrackedResultOperation(value->op_)) {
    return OverdefinedValue();
  }
  return UnknownValue();
}

auto SCCPPass::MergeInValue(mlir::Value *value, const LatticeValue &new_value) -> void {
  auto old_value = GetValueState(value);
  auto merged = MergeValues(old_value, new_value);
  if (merged.kind == old_value.kind &&
      (merged.kind != LatticeKind::CONSTANT || AreEqualConstants(merged.constant, old_value.constant))) {
    return;
  }
  value_states_[value] = merged;
  MarkUsersChanged(value);
}

auto SCCPPass::MergeValues(const LatticeValue &lhs, const LatticeValue &rhs) -> LatticeValue {
  if (lhs.kind == LatticeKind::OVERDEFINED || rhs.kind == LatticeKind::OVERDEFINED) {
    return OverdefinedValue();
  }
  if (lhs.kind == LatticeKind::UNKNOWN) {
    return rhs;
  }
  if (rhs.kind == LatticeKind::UNKNOWN) {
    return lhs;
  }
  if (AreEqualConstants(lhs.constant, rhs.constant)) {
    return lhs;
  }
  return OverdefinedValue();
}

auto SCCPPass::MarkUsersChanged(mlir::Value *value) -> void {
  auto iter = value_users_.find(value);
  if (iter == value_users_.end()) {
    return;
  }
  for (auto *user : iter->second) {
    MarkOperationChanged(user);
  }
}

auto SCCPPass::MarkOperationChanged(mlir::Operation *operation) -> void {
  if (operation != nullptr) {
    ssa_worklist_.emplace_back(operation);
  }
}

auto SCCPPass::EvaluateOperation(mlir::Operation *operation) -> LatticeValue {
  if (auto *const_int_op = mlir::DynamicOrNull<mlir::base::ConstIntOp>(operation)) {
    return ConstantInt(const_int_op->GetValue());
  }
  if (auto *const_float_op = mlir::DynamicOrNull<mlir::base::ConstFloatOp>(operation)) {
    return ConstantFloat(const_float_op->GetValue());
  }
  if (auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    auto *pointer = load_op->GetPtr();
    auto *get_ptr_op = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
    mlir::Value *root = get_ptr_op == nullptr ? pointer : get_ptr_op->GetPtr();
    if (get_ptr_op != nullptr && !get_ptr_op->GetIndices().empty()) {
      return OverdefinedValue();
    }
    auto *global = root == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(root->op_);
    if (global == nullptr || !global->IsConst() || !global->GetDimensions().empty()) {
      return OverdefinedValue();
    }
    if (global->GetIntInitValues().size() == 1) {
      return ConstantInt(global->GetIntInitValues()[0]);
    }
    if (global->GetFloatInitValues().size() == 1) {
      return ConstantFloat(global->GetFloatInitValues()[0]);
    }
    return OverdefinedValue();
  }
  if (Is<mlir::base::PhiOp>(operation)) {
    return EvaluatePhi(operation);
  }
  if (operation->operands_.empty()) {
    return OverdefinedValue();
  }

  auto lhs = GetValueState(operation->operands_[0]);
  if (operation->operands_.size() == 1) {
    if (lhs.kind == LatticeKind::UNKNOWN) {
      return UnknownValue();
    }
    if (lhs.kind == LatticeKind::OVERDEFINED) {
      return OverdefinedValue();
    }
    if (Is<mlir::base::CopyOp>(operation)) {
      return lhs;
    }
    if (Is<mlir::base::NegOp>(operation) && IsConstantInt(lhs)) {
      return ConstantInt(-GetInt(lhs));
    }
    if (Is<mlir::base::NegFOp>(operation) && IsConstantFloat(lhs)) {
      return ConstantFloat(-GetFloat(lhs));
    }
    if (Is<mlir::base::NotOp>(operation) && IsConstantBool(lhs)) {
      return ConstantBool(!GetBool(lhs));
    }
    if (Is<mlir::base::Int2FloatOp>(operation) && IsConstantInt(lhs)) {
      return ConstantFloat(static_cast<float>(GetInt(lhs)));
    }
    if (Is<mlir::base::Float2IntOp>(operation) && IsConstantFloat(lhs)) {
      return ConstantInt(static_cast<int>(GetFloat(lhs)));
    }
    if (Is<mlir::base::Bool2IntOp>(operation) && IsConstantBool(lhs)) {
      return ConstantInt(GetBool(lhs) ? 1 : 0);
    }
    if (Is<mlir::base::Int2BoolOp>(operation) && IsConstantInt(lhs)) {
      return ConstantBool(GetInt(lhs) != 0);
    }
    if (Is<mlir::base::Bool2FloatOp>(operation) && IsConstantBool(lhs)) {
      return ConstantFloat(GetBool(lhs) ? 1.0F : 0.0F);
    }
    if (Is<mlir::base::Float2BoolOp>(operation) && IsConstantFloat(lhs)) {
      return ConstantBool(GetFloat(lhs) != 0.0F);
    }
    return OverdefinedValue();
  }

  auto rhs = GetValueState(operation->operands_[1]);
  if (lhs.kind == LatticeKind::UNKNOWN || rhs.kind == LatticeKind::UNKNOWN) {
    return UnknownValue();
  }
  if (lhs.kind == LatticeKind::OVERDEFINED || rhs.kind == LatticeKind::OVERDEFINED) {
    return OverdefinedValue();
  }

  if (IsConstantInt(lhs) && IsConstantInt(rhs)) {
    int lhs_value = GetInt(lhs);
    int rhs_value = GetInt(rhs);
    if (Is<mlir::base::AddOp>(operation)) {
      return ConstantInt(lhs_value + rhs_value);
    }
    if (Is<mlir::base::SubOp>(operation)) {
      return ConstantInt(lhs_value - rhs_value);
    }
    if (Is<mlir::base::MulOp>(operation)) {
      return ConstantInt(lhs_value * rhs_value);
    }
    if (Is<mlir::base::DivOp>(operation)) {
      return rhs_value == 0 ? OverdefinedValue() : ConstantInt(lhs_value / rhs_value);
    }
    if (Is<mlir::base::ModOp>(operation)) {
      return rhs_value == 0 ? OverdefinedValue() : ConstantInt(lhs_value % rhs_value);
    }
    if (Is<mlir::base::EqualOp>(operation)) {
      return ConstantBool(lhs_value == rhs_value);
    }
    if (Is<mlir::base::NotEqualOp>(operation)) {
      return ConstantBool(lhs_value != rhs_value);
    }
    if (Is<mlir::base::LessOp>(operation)) {
      return ConstantBool(lhs_value < rhs_value);
    }
    if (Is<mlir::base::LessEqualOp>(operation)) {
      return ConstantBool(lhs_value <= rhs_value);
    }
    if (Is<mlir::base::GreaterOp>(operation)) {
      return ConstantBool(lhs_value > rhs_value);
    }
    if (Is<mlir::base::GreaterEqualOp>(operation)) {
      return ConstantBool(lhs_value >= rhs_value);
    }
  }

  if (IsConstantFloat(lhs) && IsConstantFloat(rhs)) {
    float lhs_value = GetFloat(lhs);
    float rhs_value = GetFloat(rhs);
    if (Is<mlir::base::AddFOp>(operation)) {
      return ConstantFloat(lhs_value + rhs_value);
    }
    if (Is<mlir::base::SubFOp>(operation)) {
      return ConstantFloat(lhs_value - rhs_value);
    }
    if (Is<mlir::base::MulFOp>(operation)) {
      return ConstantFloat(lhs_value * rhs_value);
    }
    if (Is<mlir::base::DivFOp>(operation)) {
      return rhs_value == 0.0F ? OverdefinedValue() : ConstantFloat(lhs_value / rhs_value);
    }
    if (Is<mlir::base::EqualFOp>(operation)) {
      return ConstantBool(lhs_value == rhs_value);
    }
    if (Is<mlir::base::NotEqualFOp>(operation)) {
      return ConstantBool(lhs_value != rhs_value);
    }
    if (Is<mlir::base::LessFOp>(operation)) {
      return ConstantBool(lhs_value < rhs_value);
    }
    if (Is<mlir::base::LessEqualFOp>(operation)) {
      return ConstantBool(lhs_value <= rhs_value);
    }
    if (Is<mlir::base::GreaterFOp>(operation)) {
      return ConstantBool(lhs_value > rhs_value);
    }
    if (Is<mlir::base::GreaterEqualFOp>(operation)) {
      return ConstantBool(lhs_value >= rhs_value);
    }
  }
  return OverdefinedValue();
}

auto SCCPPass::EvaluatePhi(mlir::Operation *operation) -> LatticeValue {
  auto *phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(operation);
  LatticeValue result = UnknownValue();
  auto incoming_values = phi_op->GetIncomingValue();
  auto incoming_blocks = phi_op->GetIncomingBlock();
  auto *phi_block = GetBlockOfOperation(operation);
  auto *phi_node = current_function_cfg_->GetNodeByBlock(phi_block);
  for (size_t i = 0; i < incoming_values.size(); ++i) {
    auto *incoming_node = current_function_cfg_->GetNodeByBlock(incoming_blocks[i]);
    if (!IsExecutableEdge(incoming_node, phi_node)) {
      continue;
    }
    result = MergeValues(result, GetValueState(incoming_values[i]));
    if (result.kind == LatticeKind::OVERDEFINED) {
      return result;
    }
  }
  return result;
}

auto SCCPPass::GetBlockOfOperation(mlir::Operation *operation) -> mlir::Block * {
  auto iter = operation_to_block_.find(operation);
  if (iter == operation_to_block_.end()) {
    return nullptr;
  }
  return iter->second;
}

auto SCCPPass::GetTerminator(mlir::Block *block) -> mlir::Operation * {
  if (block == nullptr || block->operations_.empty()) {
    return nullptr;
  }
  return block->operations_.back();
}

auto SCCPPass::IsExecutableEdge(analysis::CFGNode *from, analysis::CFGNode *to) -> bool {
  if (from == nullptr || to == nullptr) {
    return false;
  }
  return executable_edges_.count(CFGEdge{from, to}) > 0;
}

auto SCCPPass::AreEqualConstants(const ConstantValue &lhs, const ConstantValue &rhs) -> bool {
  if (lhs.value.index() != rhs.value.index()) {
    return false;
  }
  if (std::holds_alternative<int>(lhs.value)) {
    return std::get<int>(lhs.value) == std::get<int>(rhs.value);
  }
  if (std::holds_alternative<float>(lhs.value)) {
    return std::get<float>(lhs.value) == std::get<float>(rhs.value);
  }
  return std::get<bool>(lhs.value) == std::get<bool>(rhs.value);
}

auto SCCPPass::IsTrackedResultOperation(mlir::Operation *operation) -> bool {
  if (operation == nullptr || operation->results_.size() != 1) {
    return false;
  }
  if (Is<mlir::var::VarGetValOp>(operation)) {
    return true;
  }
  if (mlir::DynamicOrNull<mlir::var::VarOps>(operation) != nullptr) {
    return false;
  }
  if (Is<mlir::base::FuncCallOp>(operation) || Is<mlir::base::FuncDefOp>(operation)) {
    return false;
  }
  return true;
}

auto SCCPPass::IsReplaceableConstant(const LatticeValue &lattice_value) -> bool {
  if (lattice_value.kind != LatticeKind::CONSTANT) {
    return false;
  }
  return std::holds_alternative<int>(lattice_value.constant.value) ||
         std::holds_alternative<float>(lattice_value.constant.value);
}

auto SCCPPass::GetBoolConstant(mlir::Value *value) -> std::optional<bool> {
  auto lattice_value = GetValueState(value);
  if (!IsConstantBool(lattice_value)) {
    return std::nullopt;
  }
  return GetBool(lattice_value);
}

auto SCCPPass::CreateConstantOperation(mlir::Value *result, const ConstantValue &constant) -> mlir::Operation * {
  if (std::holds_alternative<int>(constant.value)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, std::get<int>(constant.value));
  }
  if (std::holds_alternative<float>(constant.value)) {
    return GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstFloatOp>(result, std::get<float>(constant.value));
  }
  return nullptr;
}
}  // namespace middleend::pass::transform
