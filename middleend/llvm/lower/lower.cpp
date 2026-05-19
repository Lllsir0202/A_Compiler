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
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(),
                                GET_LLVMIR_CONTEXT()->GetPtrType(GET_LLVMIR_CONTEXT()->GetIntegerType())});
  auto getarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getarray", GET_LLVMIR_CONTEXT()->GetIntegerType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetPtrType(GET_LLVMIR_CONTEXT()->GetIntegerType())});
  auto putfloatarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "putfarray", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType(),
                                GET_LLVMIR_CONTEXT()->GetPtrType(GET_LLVMIR_CONTEXT()->GetFloatType())});
  auto getfloatarray_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "getfarray", GET_LLVMIR_CONTEXT()->GetIntegerType(),
      std::vector<type::Type *>{
          GET_LLVMIR_CONTEXT()->GetTypeContext()->GetPtrType(GET_LLVMIR_CONTEXT()->GetFloatType())});
  auto starttime_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "_sysy_starttime", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType()});
  auto stoptime_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "_sysy_stoptime", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{GET_LLVMIR_CONTEXT()->GetIntegerType()});

  /** Following are the library functions we will use in the future, which include
   * 1. llvm.memset(ptr pointer, i8 value, i64 size, i1 isvolatile)
   * 2. llvm.memcpy(ptr dest, ptr src, i64 size, i1 isvolatile)
   * We will use these two functions to implement the initialization of
   * local variables and the copy instruction in the future.
   */
  auto llvm_memset_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "llvm.memset.p0.i64", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{
          GET_LLVMIR_CONTEXT()->GetTypeContext()->GetPtrType(GET_LLVMIR_CONTEXT()->GetIntegerType()),
          GET_LLVMIR_CONTEXT()->GetCharType(), GET_LLVMIR_CONTEXT()->GetLongType(),
          GET_LLVMIR_CONTEXT()->GetBoolType()});

  auto llvm_memcpy_func = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDeclareInst>(
      GET_LLVMIR_CONTEXT(), "llvm.memcpy.p0.p0.i64", GET_LLVMIR_CONTEXT()->GetVoidType(),
      std::vector<type::Type *>{
          GET_LLVMIR_CONTEXT()->GetTypeContext()->GetPtrType(GET_LLVMIR_CONTEXT()->GetIntegerType()),
          GET_LLVMIR_CONTEXT()->GetTypeContext()->GetPtrType(GET_LLVMIR_CONTEXT()->GetIntegerType()),
          GET_LLVMIR_CONTEXT()->GetLongType(), GET_LLVMIR_CONTEXT()->GetBoolType()});

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
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(llvm_memset_func);
  GET_LLVMIR_CONTEXT()->GetResourceContext()->AddLibraryFunction(llvm_memcpy_func);
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
      lower_helper_ = nullptr;
      return 1;
    } catch (const std::runtime_error &e) {
      compiler_context_->Print("Runtime error during lowering: " + std::string(e.what()));
      delete lower_helper_;
      lower_helper_ = nullptr;
      return 2;
    } catch (...) {
      compiler_context_->Print("Unknown error during lowering.");
      delete lower_helper_;
      lower_helper_ = nullptr;
      return 3;
    }
  }
  return 0;
}

}  // namespace middleend::llvm::lower