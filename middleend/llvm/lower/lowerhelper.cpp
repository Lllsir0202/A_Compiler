#include "middleend/llvm/lower/lowerhelper.h"
#include "llvm/inst/func/funcdef.h"
#include "llvm/utils/operand/operand.h"
#include "middleend/llvm/utils/factory/blockfactory.h"
#include "middleend/llvm/utils/factory/funcfactory.h"
#include "middleend/llvm/utils/factory/instfactory.h"
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_LLVMIR_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()

namespace middleend::llvm::lower {
auto LowerHelper::LowerBasicBlocks(mlir::base::FuncDefOp *func_def) -> void {
  // Get blocks from mlir base function definition
  auto blocks = func_def->GetBlocks();
  for (auto block : blocks) {
    // Create a corresponding llvm ir basic block for each mlir block
    auto basic_block = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetNewBasicBlock();
    // Map the mlir block to llvm ir basic block
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRBlockToLLVMIRBlock(block, basic_block);
    // then we will lower the instructions later
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction()->AddBasicBlock(basic_block);
  }
}

auto LowerHelper::LowerFuncDef(mlir::base::FuncDefOp *func_def) -> void {
  // function definition lowering is a bit special
  // When we meet a function definition, we will create a new llvm ir function,
  // but if the function is a library function, we will just collect and print in the future
  if (!mlir::base::IsLibraryFunction(func_def->GetName())) {
    // not a library function, create a new llvm ir function and lower the function body
    auto llvm_function =
        GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateFunction(func_def->GetName(), func_def->GetReturnType());
    // Set the current function in lower context, which will be used for instruction insertion
    GET_LLVMIR_CONTEXT()->GetLowerContext()->SetCurrentFunction(llvm_function);
    // Lower the basic blocks in the function, which will also create llvm ir basic blocks and
    LowerBasicBlocks(func_def);

    // Then create a function definition instruction to hold the function information
    auto args = func_def->GetArguments();
    std::vector<llvm::utils::Operand *> llvm_args;
    std::vector<type::Type *> llvm_arg_types;
    for (const auto *arg : args) {
      llvm_arg_types.emplace_back(arg->GetType());
      auto llvm_arg = GET_LLVMIR_CONTEXT()->GetNewRegOperand(arg->GetType());
      llvm_args.emplace_back(llvm_arg);
    }
    auto func_def_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDefInst>(
        GET_LLVMIR_CONTEXT(), func_def->GetName(), func_def->GetReturnType(), llvm_arg_types, llvm_args);
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddFunctionDef(func_def->GetName(), func_def_inst);
    // Then lower the function body
    auto blocks = func_def->GetBlocks();
    for (auto block : blocks) {
      auto llvm_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(block);
      GET_LLVMIR_CONTEXT()->GetLowerContext()->SetInsertPoint(llvm_block);
      for (auto *op : block->operations_) {
        LowerDispatch(op);
      }
    }
    // Exit the function, reset the current function in lower context
    GET_LLVMIR_CONTEXT()->GetLowerContext()->SetCurrentFunction(nullptr);
    GET_LLVMIR_CONTEXT()->GetResourceContext()->ResetFunctionLevelResource();
  }
}

auto LowerHelper::LowerDispatch(mlir::Operation *op) -> void {
  // Dispatch the lowering operation in base dialect
  if (auto func_def = dynamic_cast<mlir::base::FuncDefOp *>(op)) {
    LowerFuncDef(func_def);
  } else {
    // For other operations, we will lower them in the future
  }
}
}  // namespace middleend::llvm::lower