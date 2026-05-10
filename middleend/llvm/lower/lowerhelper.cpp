#include "middleend/llvm/lower/lowerhelper.h"
#include "llvm/inst/cast/cast.h"
#include "llvm/inst/memory/gep.h"
#include "middleend/llvm/inst/arithm/arithm.h"
#include "middleend/llvm/inst/branch/branch.h"
#include "middleend/llvm/inst/branch/phi.h"
#include "middleend/llvm/inst/branch/ret.h"
#include "middleend/llvm/inst/cmp/cmp.h"
#include "middleend/llvm/inst/func/call.h"
#include "middleend/llvm/inst/func/funcdef.h"
#include "middleend/llvm/inst/memory/globaldef.h"
#include "middleend/llvm/inst/memory/load.h"
#include "middleend/llvm/inst/memory/store.h"
#include "middleend/llvm/utils/factory/blockfactory.h"
#include "middleend/llvm/utils/factory/funcfactory.h"
#include "middleend/llvm/utils/factory/instfactory.h"
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "mlir/dialect/base/binaryop.h"
#include "mlir/dialect/var/getptr.h"
#include "mlir/utils/operation/operation.h"
#include "type/type.h"
#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_LLVMIR_CONTEXT() compiler_context_->GetIRContext()->GetLLVMIRContext()

namespace middleend::llvm::lower {

/**
 * @brief Lower Disptch with the base dialect
 */
#define LOWER_DISPATCH(OPERATION_TYPE, FUNC_NAME)                   \
  if (auto specific_op = mlir::DynamicOrNull<OPERATION_TYPE>(op)) { \
    Lower##FUNC_NAME(specific_op);                                  \
    return;                                                         \
  }

auto LowerHelper::LowerDispatch(mlir::Operation *op) -> void {
  // Dispatch the lowering operation in base dialect
  LOWER_DISPATCH(mlir::base::FuncDefOp, FuncDef)
  // For other operations, we will lower them in the future
}

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

/**
 * @brief Arithmetic operations lowering
 */
// Binary operations
#define LOWER_BINARY_OP(OP_NAME)                                                                                  \
  auto LowerHelper::Lower##OP_NAME##Op(mlir::base::OP_NAME##Op *op)->void {                                       \
    auto lhs = op->GetLHS();                                                                                      \
    auto rhs = op->GetRHS();                                                                                      \
    auto result = op->GetRes();                                                                                   \
    auto llvm_lhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(lhs);                  \
    auto llvm_rhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(rhs);                  \
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::OP_NAME##Inst>(GET_LLVMIR_CONTEXT(), \
                                                                                            llvm_lhs, llvm_rhs);  \
                                                                                                                  \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);                                 \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());              \
  }

// Int
LOWER_BINARY_OP(Add);
LOWER_BINARY_OP(Sub);
LOWER_BINARY_OP(Mul);
LOWER_BINARY_OP(Div);
LOWER_BINARY_OP(Mod);
// Float
LOWER_BINARY_OP(AddF);
LOWER_BINARY_OP(SubF);
LOWER_BINARY_OP(MulF);
LOWER_BINARY_OP(DivF);

// Comparison operations
#define ICMP_DISPTACH(OP_NAME, PREDICATE)                                                            \
  if (auto specific_op = mlir::DynamicOrNull<mlir::base::OP_NAME##Op>(op)) {                         \
    auto lhs = specific_op->GetLHS();                                                                \
    auto rhs = specific_op->GetRHS();                                                                \
    auto result = specific_op->GetRes();                                                             \
    auto llvm_lhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(lhs);     \
    auto llvm_rhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(rhs);     \
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::ICmpInst>(              \
        GET_LLVMIR_CONTEXT(), inst::CmpPredicate::PREDICATE, llvm_lhs, llvm_rhs);                    \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);                    \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult()); \
    return;                                                                                          \
  }

auto LowerHelper::LowerICmpOp(mlir::Operation *op) -> void {
  ICMP_DISPTACH(Equal, EQ)
  ICMP_DISPTACH(NotEqual, NE)
  ICMP_DISPTACH(Less, LT)
  ICMP_DISPTACH(LessEqual, LE)
  ICMP_DISPTACH(Greater, GT)
  ICMP_DISPTACH(GreaterEqual, GE)
}

auto LowerHelper::LowerFCmpOp(mlir::Operation *op) -> void {
  ICMP_DISPTACH(Equal, EQ)
  ICMP_DISPTACH(NotEqual, NE)
  ICMP_DISPTACH(Less, LT)
  ICMP_DISPTACH(LessEqual, LE)
  ICMP_DISPTACH(Greater, GT)
  ICMP_DISPTACH(GreaterEqual, GE)
}

/**
 * @brief Unary operations lowering
 */
auto LowerHelper::LowerNegOp(mlir::base::NegOp *op) -> void {
  // negation can be implemented by subtraction with zero
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetIntLiteral(0);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::SubInst>(GET_LLVMIR_CONTEXT(), zero_literal,
                                                                                    llvm_operand);

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

auto LowerHelper::LowerNegFOp(mlir::base::NegFOp *op) -> void {
  // negation can be implemented by subtraction with zero
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(0.0F);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::SubFInst>(GET_LLVMIR_CONTEXT(), zero_literal,
                                                                                     llvm_operand);

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

auto LowerHelper::LowerNotOp(mlir::base::NotOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::XorInst>(GET_LLVMIR_CONTEXT(), llvm_operand);

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

auto LowerHelper::LowerIntLiteralOp(mlir::base::ConstIntOp *op) -> void {
  auto value = op->GetValue();
  auto result = op->GetRes();
  auto int_literal = GET_LLVMIR_CONTEXT()->GetIntLiteral(value);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetIntLiteral(0);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::AddInst>(GET_LLVMIR_CONTEXT(), int_literal,
                                                                                    zero_literal);

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

auto LowerHelper::LowerFloatLiteralOp(mlir::base::ConstFloatOp *op) -> void {
  auto value = op->GetValue();
  auto result = op->GetRes();
  auto float_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(value);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(0.0F);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::AddFInst>(GET_LLVMIR_CONTEXT(),
                                                                                     float_literal, zero_literal);

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Cast operations lowering
 */

/**
 * @brief Int to Float cast,
 * using Sitofp instruction in llvm ir
 */
auto LowerHelper::LowerInt2FloatOp(mlir::base::Int2FloatOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto inst =
      GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::SitoFpInst>(GET_LLVMIR_CONTEXT(), llvm_operand);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Float to Int cast,
 * using Fptosi instruction in llvm ir
 */
auto LowerHelper::LowerFloat2IntOp(mlir::base::Float2IntOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto inst =
      GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FptoSiInst>(GET_LLVMIR_CONTEXT(), llvm_operand);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Int to Bool cast,
 * using Icmp instruction in llvm ir, comparing with zero
 */
auto LowerHelper::LowerInt2BoolOp(mlir::base::Int2BoolOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetIntLiteral(0);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::ICmpInst>(
      GET_LLVMIR_CONTEXT(), inst::CmpPredicate::NE, llvm_operand, zero_literal);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Float to Bool cast,
 * using Fcmp instruction in llvm ir, comparing with zero
 */
auto LowerHelper::LowerFloat2BoolOp(mlir::base::Float2BoolOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(0.0F);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FCmpInst>(
      GET_LLVMIR_CONTEXT(), inst::CmpPredicate::NE, llvm_operand, zero_literal);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Bool to Int cast,
 * using Zext instruction in llvm ir
 */
auto LowerHelper::LowerBool2IntOp(mlir::base::Bool2IntOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto inst =
      GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::ZextInst>(GET_LLVMIR_CONTEXT(), llvm_operand);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Bool to Float cast,
 * using Uitofp instruction in llvm ir
 */
auto LowerHelper::LowerBool2FloatOp(mlir::base::Bool2FloatOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto inst =
      GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::UitoFpInst>(GET_LLVMIR_CONTEXT(), llvm_operand);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
}

/**
 * @brief Lowering function-related operations
 */
auto LowerHelper::LowerFuncCall(mlir::base::FuncCallOp *func_call) -> void {
  auto func_name = func_call->GetName();
  auto args = func_call->GetOperands();
  auto result = func_call->GetRes();

  std::vector<llvm::utils::Operand *> llvm_args;
  for (auto *arg : args) {
    auto llvm_arg = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(arg);
    llvm_args.emplace_back(llvm_arg);
  }
  auto call_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::CallInst>(
      GET_LLVMIR_CONTEXT(), func_name, llvm_args, result->GetType());
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(call_inst);
  if (call_inst->GetResult() != nullptr) {
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, call_inst->GetResult());
  }
}

auto LowerHelper::LowerReturn(mlir::func::ReturnOp *return_op) -> void {
  auto ret = return_op->GetReturnValue();
  auto llvm_ret =
      ret == nullptr ? nullptr : GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ret);
  auto ret_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::RetInst>(GET_LLVMIR_CONTEXT(), llvm_ret);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(ret_inst);
}

/**
 * @brief Lowering branch-related operations
 */
auto LowerHelper::LowerBrOp(mlir::base::BrOp *br_op) -> void {
  auto target_block = br_op->GetTarget();
  // Since we lower all basic blocks before lowering instructions
  auto llvm_target_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(target_block);
  auto llvm_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_target_block->GetID());
  // Create a branch instruction to jump to the target block
  auto br_inst =
      GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::BrInst>(GET_LLVMIR_CONTEXT(), llvm_label_op);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(br_inst);
}

auto LowerHelper::LowerCondBrOp(mlir::base::CondBrOp *cond_br_op) -> void {
  auto condition = cond_br_op->GetCondition();
  auto true_block = cond_br_op->GetTrueTarget();
  auto false_block = cond_br_op->GetFalseTarget();

  auto llvm_condition = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(condition);
  auto llvm_true_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(true_block);
  auto llvm_false_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(false_block);
  auto llvm_true_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_true_block->GetID());
  auto llvm_false_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_false_block->GetID());

  // Create a conditional branch instruction to jump to the target blocks based on the condition
  auto br_cond_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::BrCondInst>(
      GET_LLVMIR_CONTEXT(), llvm_condition, llvm_true_label_op, llvm_false_label_op);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(br_cond_inst);
}

auto LowerHelper::LowerPhiOp(mlir::base::PhiOp *phi_op) -> void {
  // lower phi instruction
  auto result = phi_op->GetRes();
  auto incomings = phi_op->GetIncoming();
  std::vector<llvm::utils::Operand *> llvm_incoming_values;
  std::vector<llvm::utils::LabelOperand *> llvm_incoming_labels;
  for (auto &incoming : incomings) {
    auto value = incoming.first;
    auto block = incoming.second;
    auto llvm_value = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(value);
    auto llvm_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(block);
    auto llvm_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_block->GetID());
    llvm_incoming_values.emplace_back(llvm_value);
    llvm_incoming_labels.emplace_back(llvm_label_op);
  }
  auto phi_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::PhiInst>(
      GET_LLVMIR_CONTEXT(), llvm_incoming_values, llvm_incoming_labels, result->GetType());
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(phi_inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, phi_inst->GetResult());
}

/**
 * @brief Lowering memory-related operations
 */
auto LowerHelper::LowerGlobalVarDefOp(mlir::var::GlobalVarDefOp *global_var_def) -> void {
  // Lowering global variable definition is a bit special,
  // since we need record all of them, and print them in the beginning
  auto result = global_var_def->GetRes();
  auto &var_name = global_var_def->GetName();
  /**
   * @note: var_type might be array type
   */
  auto var_type = global_var_def->GetRes()->GetType();
  auto type = var_type;
  if (type->GetKind() == type::TypeKind::ARRAY) {
    type = static_cast<type::ArrayType *>(var_type)->GetElementType();
  }
  if (type->GetKind() == type::TypeKind::INT) {
    auto &inits = global_var_def->GetIntInitValues();
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
        GET_LLVMIR_CONTEXT(), var_type, var_name, global_var_def->GetDimensions(), inits);
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddGlobalVarDef(var_name, inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  } else if (type->GetKind() == type::TypeKind::FLOAT) {
    auto &inits = global_var_def->GetFloatInitValues();
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
        GET_LLVMIR_CONTEXT(), var_type, var_name, global_var_def->GetDimensions(), inits);
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddGlobalVarDef(var_name, inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  } else {
    // For other types, it is not supported
    throw std::runtime_error("Unsupported global variable type in LowerGlobalVarDefOp");
  }
}

/**
 * @note: Important Operations:
 * 1. GetPtrOp: Currently the pointer might be :
 *   a. Global variable, if it is not an array, we should directly get the value
 *   b. Global variable with array type, we should get the pointer to the first element
 *   c. Local array, get the pointer to the first element
 * 2. GetValOp: transfer to load instruction
 * 3. SetValOp: transfer to store instruction
 */
auto LowerHelper::LowerVarGetPtrOp(mlir::var::VarGetPtrOp *get_ptr_op) -> void {
  auto ptr = get_ptr_op->GetPtr();
  auto result = get_ptr_op->GetRes();
  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
  // If llvm_ptr is a global variable as well as it is not an array
  if (llvm_ptr->IsGlobal() && llvm_ptr->GetType()->GetKind() != type::TypeKind::ARRAY) {
    // directly map the result to the global variable operand
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, llvm_ptr);
  } else {
    // Otherwise, get the indices to get the pointer to the specified element
    std::vector<llvm::utils::Operand *> indices;
    // It must be an array
    for (auto &index : get_ptr_op->GetIndices()) {
      auto llvm_index = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(index);
      indices.emplace_back(llvm_index);
    }
    auto gep_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GetElementPtrInst>(
        GET_LLVMIR_CONTEXT(), result->GetType(), llvm_ptr, indices);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(gep_inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, gep_inst->GetResult());
  }
}

auto LowerHelper::LowerVarGetValOp(mlir::var::VarGetValOp *get_val_op) -> void {
  auto ptr = get_val_op->GetPtr();
  auto result = get_val_op->GetRes();
  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
  auto load_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::LoadInst>(GET_LLVMIR_CONTEXT(),
                                                                                          result->GetType(), llvm_ptr);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(load_inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, load_inst->GetResult());
}

auto LowerHelper::LowerVarSetOp(mlir::var::VarSetOp *set_val_op) -> void {
  auto ptr = set_val_op->GetPtr();
  auto value = set_val_op->GetNewValue();
  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
  auto llvm_value = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(value);
  auto store_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::StoreInst>(GET_LLVMIR_CONTEXT(),
                                                                                            llvm_value, llvm_ptr);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(store_inst);
}

// Then lowering copy instruction, but it seems to be eliminated in the future.
auto LowerHelper::LowerCopyOp(mlir::base::CopyOp *copy_op) -> void {
  auto src = copy_op->GetSrc();
  auto dst = copy_op->GetRes();
  auto llvm_src = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(src);
  // Since copy instruction is used to copy the value from src to dst,
  // we can directly map the dst to the same operand as src
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(dst, llvm_src);
}

}  // namespace middleend::llvm::lower