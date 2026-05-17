#ifndef MIDDLEEND_PASS_ANALYSIS_CFG_MLIRCFG_H
#define MIDDLEEND_PASS_ANALYSIS_CFG_MLIRCFG_H
#include <unordered_map>
#include "middleend/pass/analysis/cfg/cfg.h"
#include "pass/pass.h"

namespace middleend::mlir {
class Block;
}
namespace middleend::mlir::base {
class FuncDefOp;
}
namespace middleend::pass::analysis {

class MLIRCFGNode : public CFGNode {
 public:
  ~MLIRCFGNode() override {
    Reset();
    block_ = nullptr;
  }
  explicit MLIRCFGNode(mlir::Block *block) : CFGNode(PassStage::BASEIR), block_(block) {}
  auto GetBlock() -> mlir::Block * { return block_; }
  auto VerifyNodeType() -> bool override { return block_ != nullptr && pass_stage_ == PassStage::BASEIR; }

 private:
  mlir::Block *block_;
};

class MLIRFunctionCFG : public FunctionCFG {
 public:
  ~MLIRFunctionCFG() override {
    Reset();
    func_op_ = nullptr;
  };
  explicit MLIRFunctionCFG(mlir::base::FuncDefOp *func_op) : FunctionCFG(PassStage::BASEIR), func_op_(func_op) {}
  auto GetFuncOp() -> mlir::base::FuncDefOp * { return func_op_; }
  auto VerifyFunctionCFG() -> bool override { return func_op_ != nullptr && cfg_stage_ == PassStage::BASEIR; }
  auto GetNodeByBlock(mlir::Block *block) -> MLIRCFGNode * {
    if (block_to_node_.count(block) == 0) {
      return nullptr;
    }
    return block_to_node_[block];
  }
  auto MapBlockToNode(mlir::Block *block, MLIRCFGNode *node) -> void { block_to_node_[block] = node; }

 private:
  mlir::base::FuncDefOp *func_op_;

  std::unordered_map<mlir::Block *, MLIRCFGNode *> block_to_node_;
};

// Build cfg for mlir base dialect
class MLIRCFGBuilder : public CFGBuilder {
 public:
  explicit MLIRCFGBuilder(context::CompilerContext *compiler_context) : CFGBuilder(compiler_context) {}
  auto BuildCFG() -> void override;
  ~MLIRCFGBuilder() override { Reset(); }

 private:
  auto BuildFunctionCFG(mlir::base::FuncDefOp *func_op) -> void;
};

}  // namespace middleend::pass::analysis

#endif