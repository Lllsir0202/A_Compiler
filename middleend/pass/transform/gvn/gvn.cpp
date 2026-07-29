#include "middleend/pass/transform/gvn/gvn.h"
#include <cstdlib>
#include <cstring>
#include <utility>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/copy.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/unaryop.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"
#include "middleend/pass/analysis/memoryssa/memoryssa.h"
#include "middleend/pass/analysis/sideeffect/sideeffect.h"

#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
template <typename OperationType>
auto Is(mlir::Operation *operation) -> bool {
  return mlir::DynamicOrNull<OperationType>(operation) != nullptr;
}
}  // namespace

auto GVNPass::Run() -> void {
  PreRun();
  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  alias_analysis_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::AliasAnalysisPass>("AliasAnalysisPass");
  side_effect_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::FunctionSideEffectPass>("FunctionSideEffectPass");
  memory_ssa_ = GET_PASS_CONTEXT()->GetSpecPass<analysis::MemorySSAPass>("MemorySSAPass");
  if (cfg_pass == nullptr || dom_tree_pass == nullptr || alias_analysis_ == nullptr || side_effect_ == nullptr ||
      memory_ssa_ == nullptr) {
    throw std::logic_error(
        "GVNPass requires CFGPass, DomTreePass, AliasAnalysisPass, FunctionSideEffectPass and MemorySSAPass");
  }

  bool changed = false;
  for (auto *function_cfg : cfg_pass->Get()) {
    auto *mlir_function_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
    if (mlir_function_cfg == nullptr) {
      throw std::logic_error("GVNPass only supports MLIRFunctionCFG");
    }
    auto *dom_tree = dom_tree_pass->GetDomTree(function_cfg);
    if (dom_tree == nullptr) {
      throw std::logic_error("GVNPass requires the dominator tree of every function");
    }
    changed = RunOnFunction(mlir_function_cfg, dom_tree) || changed;
  }

  if (changed) {
    GET_PASS_CONTEXT()->InvalidateAnalysis();
  }
}

auto PostRotateGVNPass::Run() -> void {
  if (std::getenv("LOOP_ROTATE") == nullptr) {
    return;
  }
  GVNPass::Run();
}

auto PostDLEGVNPass::Run() -> void {
  if (std::getenv("POST_DLE_GVN_OFF") != nullptr) {
    return;
  }
  GVNPass::Run();
}

auto GVNPass::RunOnFunction(analysis::MLIRFunctionCFG *function_cfg, analysis::DomTree *dom_tree) -> bool {
  (void)function_cfg;
  changed_ = false;
  available_values_.clear();
  available_loads_.clear();
  auto *entry_node = dom_tree->GetEntryNode();
  if (entry_node != nullptr) {
    ProcessDomNode(entry_node);
  }
  available_values_.clear();
  available_loads_.clear();
  return changed_;
}

auto GVNPass::ProcessDomNode(analysis::DomTreeNode *dom_node) -> void {
  auto *mlir_cfg_node = dynamic_cast<analysis::MLIRCFGNode *>(dom_node->GetCFGNode());
  if (mlir_cfg_node == nullptr) {
    throw std::logic_error("GVNPass only supports MLIRCFGNode");
  }

  // Facts inserted while visiting this node; dropped when the scope is left, so an
  // available value is only reused where its definition dominates the use. No memory
  // barrier is needed: MemorySSA already accounts for merges and loop back-edges.
  std::vector<Expr> scalar_scope;
  std::vector<LoadKey> load_scope;
  ProcessBlock(mlir_cfg_node->GetBlock(), &scalar_scope, &load_scope);

  for (auto *child : dom_node->GetChildren()) {
    ProcessDomNode(child);
  }

  for (auto &key : scalar_scope) {
    available_values_.erase(key);
  }
  for (auto &key : load_scope) {
    available_loads_.erase(key);
  }
}

auto GVNPass::ProcessBlock(mlir::Block *block, std::vector<Expr> *scalar_scope, std::vector<LoadKey> *load_scope)
    -> void {
  std::vector<mlir::Operation *> kept_operations;
  kept_operations.reserve(block->operations_.size());

  for (auto *operation : block->operations_) {
    if (Is<mlir::base::CopyOp>(operation)) {
      PropagateCopy(operation);
      continue;
    }

    // Load: forward the value at its clobbering memory access; stores and calls need no
    // handling here because MemorySSA already threads the memory state through them.
    if (Is<mlir::var::VarGetValOp>(operation)) {
      if (TryForwardLoad(operation, load_scope)) {
        continue;
      }
      kept_operations.emplace_back(operation);
      continue;
    }

    Expr key;
    if (!ComputeKey(operation, &key)) {
      kept_operations.emplace_back(operation);
      continue;
    }

    auto iter = available_values_.find(key);
    if (iter != available_values_.end()) {
      EliminateRedundant(operation, iter->second);
      continue;
    }

    scalar_scope->emplace_back(key);
    available_values_.emplace(std::move(key), operation->results_[0]);
    kept_operations.emplace_back(operation);
  }

  block->operations_ = std::move(kept_operations);
}

auto GVNPass::PropagateCopy(mlir::Operation *operation) -> void {
  auto *copy_op = mlir::DynamicOrNull<mlir::base::CopyOp>(operation);
  auto *source = copy_op->GetSrc();
  auto *result = copy_op->GetRes();
  // Forward every reader of the copy result to the copy source, then detach the copy
  // itself from the source use-list before dropping it from the block.
  result->ReplaceAllUsesWith(source);
  source->RemoveUse(copy_op);
  changed_ = true;
}

auto GVNPass::EliminateRedundant(mlir::Operation *operation, mlir::Value *leader) -> void {
  operation->results_[0]->ReplaceAllUsesWith(leader);
  for (auto *operand : operation->operands_) {
    if (operand != nullptr) {
      operand->RemoveUse(operation);
    }
  }
  changed_ = true;
}

auto GVNPass::ForwardLoad(mlir::Operation *operation, mlir::Value *value) -> void {
  operation->results_[0]->ReplaceAllUsesWith(value);
  for (auto *operand : operation->operands_) {
    if (operand != nullptr) {
      operand->RemoveUse(operation);
    }
  }
  changed_ = true;
}

auto GVNPass::TryForwardLoad(mlir::Operation *load, std::vector<LoadKey> *load_scope) -> bool {
  auto *load_op = mlir::DynamicOrNull<mlir::var::VarGetValOp>(load);
  auto *pointer = load_op->GetPtr();
  auto *clobber = memory_ssa_->GetClobberingAccess(load);
  if (clobber == nullptr) {
    return false;
  }

  // Store-to-load forwarding: the clobber is a store to the very same location.
  if (clobber->GetKind() == analysis::MemoryAccess::Kind::Def) {
    auto *store_op =
        mlir::DynamicOrNull<mlir::var::VarSetOp>(static_cast<analysis::MemoryDef *>(clobber)->GetOperation());
    if (store_op != nullptr &&
        alias_analysis_->Alias(pointer, store_op->GetPtr()) == analysis::AliasResult::MustAlias) {
      ForwardLoad(load, store_op->GetNewValue());
      return true;
    }
  }

  // Load-to-load reuse: an earlier load in scope reads from the same clobber and pointer,
  // hence the same value, and (being in scope) dominates this load.
  LoadKey key{clobber, pointer};
  auto iter = available_loads_.find(key);
  if (iter != available_loads_.end()) {
    ForwardLoad(load, iter->second);
    return true;
  }
  available_loads_.emplace(key, load_op->GetRes());
  load_scope->emplace_back(key);
  return false;
}

auto GVNPass::ComputeKey(mlir::Operation *operation, Expr *key) -> bool {
  if (auto *const_int_op = mlir::DynamicOrNull<mlir::base::ConstIntOp>(operation)) {
    key->opcode = &typeid(*operation);
    key->has_immediate = true;
    key->immediate = static_cast<uint64_t>(static_cast<int64_t>(const_int_op->GetValue()));
    return true;
  }
  if (auto *const_float_op = mlir::DynamicOrNull<mlir::base::ConstFloatOp>(operation)) {
    key->opcode = &typeid(*operation);
    key->has_immediate = true;
    float value = const_float_op->GetValue();
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    key->immediate = bits;
    return true;
  }

  if (auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(operation)) {
    if (!side_effect_->IsReferentiallyTransparent(call->GetName())) {
      return false;
    }
    key->opcode = &typeid(*operation);
    key->callee = call->GetName();
    key->operands = operation->operands_;
    return operation->results_.size() == 1;
  }

  if (!IsPure(operation) || operation->results_.size() != 1) {
    return false;
  }

  key->opcode = &typeid(*operation);
  key->operands = operation->operands_;
  // Canonicalize commutative operands so that "a op b" and "b op a" share one key.
  if (IsCommutative(operation) && key->operands.size() == 2 && key->operands[0] > key->operands[1]) {
    std::swap(key->operands[0], key->operands[1]);
  }
  return true;
}

auto GVNPass::IsPure(mlir::Operation *operation) -> bool {
  // Integer arithmetic and comparisons.
  if (Is<mlir::base::AddOp>(operation) || Is<mlir::base::SubOp>(operation) || Is<mlir::base::MulOp>(operation) ||
      Is<mlir::base::DivOp>(operation) || Is<mlir::base::ModOp>(operation) || Is<mlir::base::ShlOp>(operation) ||
      Is<mlir::base::LShrOp>(operation) || Is<mlir::base::AShrOp>(operation) ||
      Is<mlir::base::EqualOp>(operation) ||
      Is<mlir::base::NotEqualOp>(operation) || Is<mlir::base::LessOp>(operation) ||
      Is<mlir::base::LessEqualOp>(operation) || Is<mlir::base::GreaterOp>(operation) ||
      Is<mlir::base::GreaterEqualOp>(operation)) {
    return true;
  }
  // Floating-point arithmetic and comparisons.
  if (Is<mlir::base::AddFOp>(operation) || Is<mlir::base::SubFOp>(operation) || Is<mlir::base::MulFOp>(operation) ||
      Is<mlir::base::DivFOp>(operation) || Is<mlir::base::EqualFOp>(operation) ||
      Is<mlir::base::NotEqualFOp>(operation) || Is<mlir::base::LessFOp>(operation) ||
      Is<mlir::base::LessEqualFOp>(operation) || Is<mlir::base::GreaterFOp>(operation) ||
      Is<mlir::base::GreaterEqualFOp>(operation)) {
    return true;
  }
  // Unary operations.
  if (Is<mlir::base::NegOp>(operation) || Is<mlir::base::NegFOp>(operation) || Is<mlir::base::NotOp>(operation)) {
    return true;
  }
  // Type casts.
  if (Is<mlir::base::Int2FloatOp>(operation) || Is<mlir::base::Float2IntOp>(operation) ||
      Is<mlir::base::Bool2IntOp>(operation) || Is<mlir::base::Int2BoolOp>(operation) ||
      Is<mlir::base::Bool2FloatOp>(operation) || Is<mlir::base::Float2BoolOp>(operation)) {
    return true;
  }
  // Address computation: same base pointer and indices always yield the same address.
  if (Is<mlir::var::VarGetPtrOp>(operation)) {
    return true;
  }
  return false;
}

auto GVNPass::IsCommutative(mlir::Operation *operation) -> bool {
  return Is<mlir::base::AddOp>(operation) || Is<mlir::base::MulOp>(operation) || Is<mlir::base::AddFOp>(operation) ||
         Is<mlir::base::MulFOp>(operation) || Is<mlir::base::EqualOp>(operation) ||
         Is<mlir::base::NotEqualOp>(operation) || Is<mlir::base::EqualFOp>(operation) ||
         Is<mlir::base::NotEqualFOp>(operation);
}

}  // namespace middleend::pass::transform
