#include "middleend/llvm/lower/lower.h"
#include <stdexcept>
#include "context/llvmir_context.h"
#include "llvm/inst/func/funcdeclare.h"
#include "middleend/llvm/utils/factory/modulefactory.h"
#define GET_LLVMIR_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
namespace middleend::llvm::lower {
/**
 * @brief Register Library functions
 */
auto LLVMLower::RegisterLibraryFunctions() -> void {
  auto putint_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putint", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType()});

  auto getint_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getint", GET_LLVMIR_CONTEXT()->GetIntegerType(), std::vector<type::Type *>{});

  auto putfloat_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putfloat", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetFloatType()});
  auto getfloat_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getfloat", GET_LLVMIR_CONTEXT()->GetFloatType(), std::vector<type::Type *>{});

  auto getch_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getch", GET_LLVMIR_CONTEXT()->GetIntegerType(), std::vector<type::Type *>{});
  auto putch_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putch", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType()});

  auto putarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putarray", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(), GET_LLVMIR_CONTEXT()->GetIntegerType()});
  auto getarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getarray", GET_LLVMIR_CONTEXT()->GetIntegerType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(), GET_LLVMIR_CONTEXT()->GetIntegerType()});
  auto putfloatarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putfarray", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(), GET_LLVMIR_CONTEXT()->GetIntegerType()});
  auto getfloatarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getfarray", GET_LLVMIR_CONTEXT()->GetFloatType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(), GET_LLVMIR_CONTEXT()->GetIntegerType()});
  auto starttime_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "starttime", GET_LLVMIR_CONTEXT()->GetVoidType(), std::vector<type::Type *>{});
  auto stoptime_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "stoptime", GET_LLVMIR_CONTEXT()->GetVoidType(), std::vector<type::Type *>{});

  // Register the library functions in the module
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(putint_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(getint_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(putfloat_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(getfloat_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(getch_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(putch_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(putarray_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(getarray_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(putfloatarray_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(getfloatarray_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(starttime_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(stoptime_func);
}

auto LLVMLower::Lower() -> int {
  // Then create the module
  auto module = utils::factory::ModuleFactory::CreateModule();
  GET_LLVMIR_CONTEXT()->GetResourceContext()->SetModule(module);

  // First, register all library functions
  RegisterLibraryFunctions();

  auto module_op = GET_MLIR_CONTEXT()->GetModuleOp();
  // Traverse the module and lower each function definition
  for (auto &op : module_op->GetBlock()->operations_) {
    try {
      lower_helper_->LowerDispatch(op);
    } catch (const std::logic_error &e) {
      compiler_context_->Print("Error during lowering: " + std::string(e.what()));
      delete lower_helper_;
      delete module;
      lower_helper_ = nullptr;
      return 1;
    } catch (const std::runtime_error &e) {
      compiler_context_->Print("Runtime error during lowering: " + std::string(e.what()));
      delete lower_helper_;
      delete module;
      lower_helper_ = nullptr;
      return 2;
    } catch (...) {
      compiler_context_->Print("Unknown error during lowering.");
      delete lower_helper_;
      delete module;
      lower_helper_ = nullptr;
      return 3;
    }
  }
  return 0;
}

}  // namespace middleend::llvm::lower