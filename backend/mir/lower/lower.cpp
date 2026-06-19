#include "backend/mir/lower/lower.h"
#include "backend/mir/inst/func/funcdeclare.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/inst/memory/globaldef.h"
#include "backend/mir/utils/factory/modulefactory.h"
#include "middleend/llvm/inst/func/funcdeclare.h"
#include "middleend/llvm/inst/func/funcdef.h"
#include "middleend/llvm/inst/memory/globaldef.h"
#include "middleend/llvm/utils/operand/literaloperand.h"
#include "utils/type/type.h"

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
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(param, mir_arg);
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
            GET_MIR_CONTEXT(), global_def->GetResult()->GetType(), var_type, var_name, global_def->GetDimensions(),
            initial_values);
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(global_def->GetResult(),
                                                                         mir_global_def->GetResult());
      } else if (!global_def->GetFloatInitialValues().empty()) {
        auto initial_values = global_def->GetFloatInitialValues();
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), global_def->GetResult()->GetType(), var_type, var_name, global_def->GetDimensions(),
            initial_values);
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(global_def->GetResult(),
                                                                         mir_global_def->GetResult());
      }
      // No initializer
      else if (var_type->GetKind() == type::TypeKind::INT) {
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), global_def->GetResult()->GetType(), var_type, var_name, global_def->GetDimensions(),
            std::vector<int>());
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(global_def->GetResult(),
                                                                         mir_global_def->GetResult());
      } else if (var_type->GetKind() == type::TypeKind::FLOAT) {
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), global_def->GetResult()->GetType(), var_type, var_name, global_def->GetDimensions(),
            std::vector<float>());
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(global_def->GetResult(),
                                                                         mir_global_def->GetResult());
      } else if (var_type->GetKind() == type::TypeKind::FLOAT) {
        auto mir_global_def = GET_MIR_CONTEXT()->GetResourceContext()->CreateInstruction<inst::GlobalDefInst>(
            GET_MIR_CONTEXT(), global_def->GetResult()->GetType(), var_type, var_name, global_def->GetDimensions(),
            std::vector<float>());
        GET_MIR_CONTEXT()->AddGlobalDefInst(mir_global_def);
        GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(global_def->GetResult(),
                                                                         mir_global_def->GetResult());
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

    // Handle all constant literal operands
    for (const auto &literal : GET_LLVM_CONTEXT()->GetResourceContext()->GetAllOperands()) {
      if (literal->IsLiteral()) {
        // Create the corresponding literal operand in MIR context
        if (literal->IsInt32()) {
          auto int_literal =
              GET_MIR_CONTEXT()->GetIntLiteral(static_cast<middleend::llvm::utils::IntLiteral *>(literal)->GetValue());
          GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(literal, int_literal);
        } else if (literal->IsFloat()) {
          auto float_literal = GET_MIR_CONTEXT()->GetFloatLiteral(
              static_cast<middleend::llvm::utils::FloatLiteral *>(literal)->GetValue());
          GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(literal, float_literal);
        } else if (literal->IsBool()) {
          auto bool_literal = GET_MIR_CONTEXT()->GetBoolLiteral(
              static_cast<middleend::llvm::utils::BoolLiteral *>(literal)->GetValue());
          GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(literal, bool_literal);
        } else if (literal->IsLong()) {
          auto long_literal = GET_MIR_CONTEXT()->GetLongLiteral(
              static_cast<middleend::llvm::utils::LongLiteral *>(literal)->GetValue());
          GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(literal, long_literal);
        } else if (literal->IsChar()) {
          auto char_literal = GET_MIR_CONTEXT()->GetCharLiteral(
              static_cast<middleend::llvm::utils::CharLiteral *>(literal)->GetValue());
          GET_MIR_CONTEXT()->GetLowerContext()->MapLLVMOperandToMIROperand(literal, char_literal);
        }
      }
    }

    // Then lower the function bodies
    for (const auto &function : llvm_module->GetFunctions()) {
      lower_helper_->LowerFunction(function);
    }

    // Then eliminate phi instructions
    for (auto &fuction : GET_MIR_CONTEXT()->GetModule()->GetFunctions()) {
      phi_elimination_->EliminatePhi(fuction);
    }

  } catch (std::exception &e) {
    compiler_context_->Print("Error during module lowering: " + std::string(e.what()));
    return -1;
  }
  return 0;
}

}  // namespace backend::lower