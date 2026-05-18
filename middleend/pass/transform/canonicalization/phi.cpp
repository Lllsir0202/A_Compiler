#include "middleend/pass/transform/canonicalization/phi.h"
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/base/phi.h"
#include "mlir/utils/operation/operation.h"
#include "mlir/utils/struct/block.h"
#include "mlir/utils/value/value.h"
#include "pass/analysis/cfg/cfg.h"
#include "pass/analysis/cfg/mlircfg.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_LLVM_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
namespace middleend::pass::transform {
auto PhiCanonicalizationPass::Run() -> void {
  PreRun();
  if (pass_stage_ == PassStage::BASEIR) {
    CanonicalizePhiInFunction();
  } else if (pass_stage_ == PassStage::LLVMIR) {
    CanonicalizePhiInLLVMFunction();
  } else {
    throw std::logic_error("PhiCanonicalizationPass only supports BASEIR or LLVMIR stage");
  }
}

auto PhiCanonicalizationPass::CanonicalizePhiInFunction() -> void {
  auto cfgpass = compiler_context_->GetPassContext()->GetSpecPass<analysis::CFGPass>("CFGPass");
  if (cfgpass->GetPassStage() != PassStage::BASEIR) {
    throw std::logic_error("CFGPass should be run before PhiCanonicalizationPass");
  }
  auto cfgs = cfgpass->Get();
  for (auto cfg : cfgs) {
    if (auto mlir_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(cfg)) {
      // Update and canonicalize phi nodes in the block
      // Deal with phi nodes per function basis.
      std::unordered_set<mlir::Operation *> to_remove_ops;
      std::vector<mlir::base::PhiOp *> phi_ops;
      for (auto cfg_node : mlir_cfg->GetNodes()) {
        if (auto mlir_block = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node)) {
          // For each phi node in the block, do canonicalization
          auto block = mlir_block->GetBlock();
          auto predecessors = mlir_block->GetPredecessors();
          // build a set of predecessor blocks for easy lookup
          std::unordered_set<mlir::Block *> predecessor_set;
          for (auto predecessor : predecessors) {
            predecessor_set.insert(static_cast<analysis::MLIRCFGNode *>(predecessor)->GetBlock());
          }
          for (auto op : block->operations_) {
            if (auto phi_op = mlir::DynamicOrNull<mlir::base::PhiOp>(op)) {
              auto values = phi_op->GetIncomingValue();
              for (auto value : values) {
                value->RemoveUse(phi_op);
              }
              auto blocks = phi_op->GetIncomingBlock();
              std::vector<mlir::Value *> new_values;
              std::vector<mlir::Block *> new_blocks;
              std::unordered_set<mlir::Block *> seen_blocks;
              std::unordered_map<mlir::Block *, mlir::Value *> block_to_value;
              for (size_t i = 0; i < values.size(); ++i) {
                auto value = values[i];
                auto incoming_block = blocks[i];
                if (compiler_context_->GetPassContext()->IsDeletedValue(value)) {
                  // the value has been deleted
                  // printf("value is %s%zu, but it has been deleted\n", value->name_.c_str(), value->num_);
                  continue;
                }
                if (seen_blocks.count(incoming_block) > 0) {
                  // it must be the same value for the same block
                  if (block_to_value[incoming_block] != value) {
                    throw std::logic_error("Inconsistent phi node incoming values for the same block");
                  }
                  continue;
                }
                if (predecessor_set.count(incoming_block) > 0) {
                  new_values.emplace_back(value);
                  new_blocks.emplace_back(incoming_block);
                  // printf("Incoming block %s is a predecessor, value is %s%zu\n", incoming_block->Name().c_str(),
                  //        value->name_.c_str(), value->num_);

                  // printf("before adding , new use index is %zu, new blocks size is %zu\n", new_values.size() - 1,
                  //        new_blocks.size());
                  value->AddUse(phi_op, new_values.size() - 1);
                  seen_blocks.insert(incoming_block);
                  block_to_value[incoming_block] = value;
                }
              }
              // Update all phi incomings first
              // Set the new incomings to the phi node
              phi_op->SetIncomings(new_values, new_blocks);
              phi_ops.emplace_back(phi_op);
            }
          }
        } else {
          throw std::logic_error("Unsupported CFG node type in PhiCanonicalizationPass");
        }
      }

      // Record and replace the phi nodes
      for (auto cfg_node : mlir_cfg->GetNodes()) {
        if (auto mlir_block = dynamic_cast<analysis::MLIRCFGNode *>(cfg_node)) {
          auto block = mlir_block->GetBlock();
          // Deal with the phi nodes that have only one incoming value
          // Mark all the phi nodes we need to remove
          for (auto phi_op : phi_ops) {
            // 1 or 0 incoming
            if (phi_op->GetIncomingSize() < 2) {
              to_remove_ops.insert(phi_op);
              continue;
            }
            // All same incoming
            auto incoming_values = phi_op->GetIncomingValue();
            bool all_same = true;
            for (size_t i = 1; i < incoming_values.size(); ++i) {
              if (incoming_values[i] != incoming_values[0]) {
                all_same = false;
                break;
              }
            }
            if (all_same) {
              to_remove_ops.insert(phi_op);
              continue;
            }
          }
          for (auto phi_op : phi_ops) {
            if (to_remove_ops.count(phi_op) > 0) {
              auto value = phi_op->GetIncomingValue()[0];
              // if the value's op is also a phi node, and it is in the to_remove_ops,
              // we should get the final value after replacement
              while (auto value_op = mlir::DynamicOrNull<mlir::base::PhiOp>(value->op_)) {
                if (to_remove_ops.count(value_op) != 0) {
                  value = value_op->GetIncomingValue()[0];
                } else {
                  break;
                }
              }
              // Get final value after replacement, replace all uses of the phi node with the value
              phi_op->GetRes()->ReplaceAllUsesWith(value);
            }
          }

          // Remove the phi nodes that have been replaced
          std::vector<mlir::Operation *> new_ops;
          new_ops.reserve(block->operations_.size());
          for (auto op : block->operations_) {
            if (to_remove_ops.count(op) == 0) {
              new_ops.emplace_back(op);
            }
          }
          // Set the new operations to the block
          block->operations_ = std::move(new_ops);
        }
      }
    } else {
      throw std::logic_error("Unsupported CFG type in PhiCanonicalizationPass");
    }
  }
}

auto PhiCanonicalizationPass::CanonicalizePhiInLLVMFunction() -> void {}

}  // namespace middleend::pass::transform