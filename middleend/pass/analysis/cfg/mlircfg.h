#ifndef MIDDLEEND_PASS_ANALYSIS_CFG_MLIRCFG_H
#define MIDDLEEND_PASS_ANALYSIS_CFG_MLIRCFG_H
#include "middleend/pass/analysis/cfg/cfg.h"
namespace middleend::mlir {
class Block;
}
namespace middleend::mlir::base {
class FuncDefOp;
}
namespace middleend::pass::analysis {

class MLIRCFGNode : public CFGNode {
 public:
  ~MLIRCFGNode() override { block_ = nullptr; }
  explicit MLIRCFGNode(mlir::Block *block) : CFGNode(CFGType::MLIR), block_(block) {}
  auto GetBlock() -> mlir::Block * { return block_; }
  auto VerifyNodeType() -> bool override { return block_ != nullptr && node_type_ == CFGType::MLIR; }

 private:
  mlir::Block *block_;
};

class MLIRFunctionCFG : public FunctionCFG {
 public:
  ~MLIRFunctionCFG() override { func_op_ = nullptr; };
  explicit MLIRFunctionCFG(mlir::base::FuncDefOp *func_op) : FunctionCFG(CFGType::MLIR), func_op_(func_op) {}
  auto GetFuncOp() -> mlir::base::FuncDefOp * { return func_op_; }
  auto VerifyFunctionCFG() -> bool override { return func_op_ != nullptr && cfg_type_ == CFGType::MLIR; }

 private:
  mlir::base::FuncDefOp *func_op_;
};

// Build cfg for mlir base dialect
class MLIRCFGBuilder : public CFGBuilder {
 public:
  explicit MLIRCFGBuilder(context::CompilerContext *compiler_context) : CFGBuilder(compiler_context) {}
  auto BuildCFG() -> void override;
  ~MLIRCFGBuilder() override {
    for (auto cfg : function_cfgs_) {
      delete cfg;
    }
    function_cfgs_.clear();
  }

 private:
  auto BuildFunctionCFG(mlir::base::FuncDefOp *func_op) -> void;
};

}  // namespace middleend::pass::analysis

#endif