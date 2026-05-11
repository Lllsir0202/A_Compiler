#ifndef MIDDLEEND_PASS_ANALYSIS_CFG_LLVMCFG_H
#define MIDDLEEND_PASS_ANALYSIS_CFG_LLVMCFG_H
#include "middleend/llvm/utils/struct/block.h"
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "pass/pass.h"
namespace middleend::pass::analysis {
class LLVMIRCFGNode : public CFGNode {
 public:
  explicit LLVMIRCFGNode(middleend::llvm::utils::BasicBlock *block) : CFGNode(PassStage::LLVMIR), block_(block) {}
  auto GetBlock() -> middleend::llvm::utils::BasicBlock * { return block_; }
  auto VerifyNodeType() -> bool override { return block_ != nullptr && pass_stage_ == PassStage::LLVMIR; }

 private:
  middleend::llvm::utils::BasicBlock *block_;
};

class LLVMIRFunctionCFG : public FunctionCFG {
 public:
  explicit LLVMIRFunctionCFG(middleend::llvm::utils::Function *function)
      : FunctionCFG(PassStage::LLVMIR), function_(function) {}
  auto GetFunction() -> middleend::llvm::utils::Function * { return function_; }
  auto VerifyFunctionCFG() -> bool override { return function_ != nullptr && cfg_stage_ == PassStage::LLVMIR; }

 private:
  middleend::llvm::utils::Function *function_;
};

// Build cfg for llvmir
class LLVMIRCFGBuilder : public CFGBuilder {
 public:
  explicit LLVMIRCFGBuilder(context::CompilerContext *compiler_context) : CFGBuilder(compiler_context) {}
  auto BuildCFG() -> void override;
};

}  // namespace middleend::pass::analysis

#endif