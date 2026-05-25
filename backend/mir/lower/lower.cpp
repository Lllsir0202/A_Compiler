#include "backend/mir/lower/lower.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/utils/factory/modulefactory.h"
#include "middleend/llvm/inst/func/funcdeclare.h"
#include "middleend/llvm/inst/func/funcdef.h"
#include "middleend/llvm/inst/memory/globaldef.h"

#define GET_LLVM_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()
#define GET_MIR_CONTEXT() compiler_context_->GetMIRContext()

namespace backend::lower {
auto MIRLower::Lower() -> int {
  try {
    auto module = utils::factory::ModuleFactory::CreateModule();
    GET_MIR_CONTEXT()->SetModule(module);
    auto llvm_module = GET_LLVM_CONTEXT()->GetModule();
    auto functions = llvm_module->GetFunctions();
    auto func_defs = llvm_module->GetFunctionDefs();
    for (const auto &[_, func_def] : func_defs) {
      // Lower function definition
      auto func_name = func_def->GetFuncName();
      auto return_type = func_def->GetReturnType();
      auto params = func_def->GetArgOperands();
      auto mir_args = std::vector<utils::Operand *>();
      for (auto param : params) {
        auto param_type = param->GetType();
        auto mir_arg = GET_MIR_CONTEXT()->GetNewRegOperand(param_type);
        mir_args.emplace_back(mir_arg);
      }
      auto mir_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::FuncDefInst>(
          GET_MIR_CONTEXT(), func_name, return_type, mir_args);
      // Add function definition inst
      GET_MIR_CONTEXT()->AddFuncDefInst(mir_def);
    }

    // Create global variable definitions
    auto global_defs = llvm_module->GetGlobalVarDefs();
    for (const auto &[_, global_def] : global_defs) {
      auto var_name = global_def->GetName();
      auto var_type = global_def->GetAllocatedType();
      if (!global_def->GetIntInitialValues().empty()) {
        auto initial_values = global_def->GetIntInitialValues();
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), var_name, var_type, initial_values);
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
      }
      if (!global_def->GetFloatInitialValues().empty()) {
        auto initial_values = global_def->GetFloatInitialValues();
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), var_name, var_type, initial_values);
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
      }
    }

    // Lower FuncDeclare instructions
    auto func_declares = llvm_module->GetLibraryFuncDefs();
    for (const auto &func_declare : func_declares) {
      auto func_name = func_declare->GetFuncName();
      auto return_type = func_declare->GetReturnType();
      auto types = func_declare->GetArgTypes();
      auto mir_declare = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::FuncDeclareInst>(
          GET_MIR_CONTEXT(), func_name, return_type, types);
      // Add function declaration inst
      GET_MIR_CONTEXT()->AddFuncDeclareInst(mir_declare);
    }

    // Then lower the function bodies
    for (const auto &function : llvm_module->GetFunctions()) {
      lower_helper_->LowerFunction(function);
    }

  } catch (std::exception &e) {
    compiler_context_->ReportError("Error during module lowering: " + std::string(e.what()));
    return -1;
  }
  return 0;
}

}  // namespace backend::lower