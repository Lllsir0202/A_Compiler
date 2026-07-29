#include "middleend/llvm/lower/lowerhelper.h"
#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "llvm/inst/opkind.h"
#include "llvm/utils/operand/literaloperand.h"
#include "llvm/utils/operand/regoperand.h"
#include "middleend/llvm/inst/arithm/arithm.h"
#include "middleend/llvm/inst/branch/branch.h"
#include "middleend/llvm/inst/branch/phi.h"
#include "middleend/llvm/inst/branch/ret.h"
#include "middleend/llvm/inst/cast/cast.h"
#include "middleend/llvm/inst/cmp/cmp.h"
#include "middleend/llvm/inst/func/call.h"
#include "middleend/llvm/inst/func/funcdef.h"
#include "middleend/llvm/inst/inst.h"
#include "middleend/llvm/inst/memory/allocainst.h"
#include "middleend/llvm/inst/memory/gep.h"
#include "middleend/llvm/inst/memory/globaldef.h"
#include "middleend/llvm/inst/memory/load.h"
#include "middleend/llvm/inst/memory/store.h"
#include "middleend/llvm/inst/pseudo/pseudo.h"
#include "middleend/llvm/utils/factory/blockfactory.h"
#include "middleend/llvm/utils/factory/funcfactory.h"
#include "middleend/llvm/utils/factory/instfactory.h"
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "middleend/llvm/utils/struct/block.h"
#include "middleend/mlir/dialect/base/binaryop.h"
#include "middleend/mlir/dialect/base/branch.h"
#include "middleend/mlir/dialect/base/cast.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "utils/type/type.h"
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
  // Function-related operations
  try {
    LOWER_DISPATCH(mlir::base::FuncDefOp, FuncDefOp)
    LOWER_DISPATCH(mlir::base::FuncCallOp, FuncCallOp)
    LOWER_DISPATCH(mlir::func::ReturnOp, ReturnOp)
    // Branch
    LOWER_DISPATCH(mlir::base::BrOp, BrOp)
    LOWER_DISPATCH(mlir::base::CondBrOp, CondBrOp)
    LOWER_DISPATCH(mlir::base::PhiOp, PhiOp)
    // Arithmetic operations
    LOWER_DISPATCH(mlir::base::AddOp, AddOp)
    LOWER_DISPATCH(mlir::base::SubOp, SubOp)
    LOWER_DISPATCH(mlir::base::MulOp, MulOp)
    LOWER_DISPATCH(mlir::base::DivOp, DivOp)
    LOWER_DISPATCH(mlir::base::ModOp, ModOp)
    LOWER_DISPATCH(mlir::base::ShlOp, ShlOp)
    LOWER_DISPATCH(mlir::base::LShrOp, LShrOp)
    LOWER_DISPATCH(mlir::base::AShrOp, AShrOp)
    LOWER_DISPATCH(mlir::base::AddFOp, AddFOp)
    LOWER_DISPATCH(mlir::base::SubFOp, SubFOp)
    LOWER_DISPATCH(mlir::base::MulFOp, MulFOp)
    LOWER_DISPATCH(mlir::base::DivFOp, DivFOp)
    // Lower comparison operations
    LOWER_DISPATCH(mlir::base::EqualOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::NotEqualOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::LessOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::LessEqualOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::GreaterOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::GreaterEqualOp, ICmpOp)
    LOWER_DISPATCH(mlir::base::EqualFOp, FCmpOp)
    LOWER_DISPATCH(mlir::base::NotEqualFOp, FCmpOp)
    LOWER_DISPATCH(mlir::base::LessFOp, FCmpOp)
    LOWER_DISPATCH(mlir::base::LessEqualFOp, FCmpOp)
    LOWER_DISPATCH(mlir::base::GreaterFOp, FCmpOp)
    LOWER_DISPATCH(mlir::base::GreaterEqualFOp, FCmpOp)
    // Unary operations
    LOWER_DISPATCH(mlir::base::NegOp, NegOp)
    LOWER_DISPATCH(mlir::base::NegFOp, NegFOp)
    LOWER_DISPATCH(mlir::base::NotOp, NotOp)
    LOWER_DISPATCH(mlir::base::ConstIntOp, IntLiteralOp)
    LOWER_DISPATCH(mlir::base::ConstFloatOp, FloatLiteralOp)
    // Cast operations
    LOWER_DISPATCH(mlir::base::Int2FloatOp, Int2FloatOp)
    LOWER_DISPATCH(mlir::base::Float2IntOp, Float2IntOp)
    LOWER_DISPATCH(mlir::base::Int2BoolOp, Int2BoolOp)
    LOWER_DISPATCH(mlir::base::Float2BoolOp, Float2BoolOp)
    LOWER_DISPATCH(mlir::base::Bool2IntOp, Bool2IntOp)
    LOWER_DISPATCH(mlir::base::Bool2FloatOp, Bool2FloatOp)
    // Variable-related operations
    LOWER_DISPATCH(middleend::mlir::var::VarGetPtrOp, VarGetPtrOp)
    LOWER_DISPATCH(middleend::mlir::var::VarDefOp, VarDefOp)
    LOWER_DISPATCH(middleend::mlir::var::GlobalVarDefOp, GlobalVarDefOp)
    LOWER_DISPATCH(middleend::mlir::var::VarGetValOp, VarGetValOp)
    LOWER_DISPATCH(middleend::mlir::var::VarSetOp, VarSetOp)
    // Copy operation
    LOWER_DISPATCH(middleend::mlir::base::CopyOp, CopyOp)
  } catch (const std::logic_error &e) {
    throw std::logic_error("Error in LowerDispatch for operation: " + op->Name() + ", error message: " + e.what());
  } catch (const std::runtime_error &e) {
    throw std::runtime_error("Runtime error in LowerDispatch for operation: " + op->Name() +
                             ", error message: " + e.what());
  }
  // If not recognized, throw an error
  throw std::logic_error("Unsupported operation type in LowerDispatch: " + op->Name());
}

auto LowerHelper::LowerBasicBlocks(mlir::base::FuncDefOp *func_def) -> void {
  for (auto block : func_def->GetBlocks()) {
    // Create a corresponding llvm ir basic block for each mlir block
    auto function = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction();
    auto basic_block = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetNewBasicBlock(function);
    // Map the mlir block to llvm ir basic block
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRBlockToLLVMIRBlock(block, basic_block);
    // then we will lower the instructions later
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction()->AddBasicBlock(basic_block);
  }
}

auto LowerHelper::LowerFuncDefOp(mlir::base::FuncDefOp *func_def) -> void {
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
    for (auto *arg : args) {
      llvm_arg_types.emplace_back(arg->GetType());
      auto llvm_arg = GET_LLVMIR_CONTEXT()->GetNewRegOperand(arg->GetType());
      GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(arg, llvm_arg);
      llvm_args.emplace_back(llvm_arg);
    }
    auto func_def_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FuncDefInst>(
        GET_LLVMIR_CONTEXT(), func_def->GetName(), func_def->GetReturnType(), llvm_arg_types, llvm_args);
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddFunctionDef(func_def->GetName(), func_def_inst);
    // Then lower the function body

    auto blocks = func_def->GetBlocks();

    // Create all alloca instructions in the entry block,
    // and then initialize in the point of the original variable definition
    for (auto block : blocks) {
      for (auto *op : block->operations_) {
        if (auto var_def_op = mlir::DynamicOrNull<middleend::mlir::var::VarDefOp>(op)) {
          auto allocated_type = var_def_op->GetRes()->type_;
          // Get allocated type
          // it should only be array or basic type, no ptr type included
          while (allocated_type->GetKind() == type::TypeKind::ARRAY) {
            auto array_type = dynamic_cast<type::ArrayType *>(allocated_type);
            allocated_type = array_type->GetElementType();
          }
          auto result_type = var_def_op->GetRes()->GetType();
          auto alloca_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::AllocaInst>(
              GET_LLVMIR_CONTEXT(), result_type, allocated_type, var_def_op->GetDimensions());
          GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction()->GetEntryBlock()->AddInstBack(alloca_inst);
          GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(var_def_op->GetRes(),
                                                                               alloca_inst->GetResult());
          alloca_inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction()->GetEntryBlock());
        }
      }
    }

    for (auto block : blocks) {
      auto llvm_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(block);
      GET_LLVMIR_CONTEXT()->GetLowerContext()->SetInsertPoint(llvm_block);
      for (auto *op : block->operations_) {
        LowerDispatch(op);
      }
    }

    // After lowering the function body,
    // we will update the pending phi instructions
    // with the incoming values and corresponding blocks
    size_t i = 0;
    while (!pending_phi_insts_.empty() && !pending_phi_ops_.empty()) {
      auto phi_inst = pending_phi_insts_.front();
      pending_phi_insts_.pop();
      auto phi_op = pending_phi_ops_.front();
      pending_phi_ops_.pop();
      auto incoming_values = phi_op->GetIncomingValue();
      auto incoming_blocks = phi_op->GetIncomingBlock();
      for (size_t j = 0; j < incoming_values.size(); ++j) {
        auto incoming_value = incoming_values[j];
        auto incoming_block = incoming_blocks[j];
        auto llvm_incoming_value =
            GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(incoming_value);
        if (llvm_incoming_value == nullptr) {
          printf("Incoming value num is : %s%zu\n", incoming_value->name_.c_str(), incoming_value->num_);
          throw std::logic_error("Failed to get LLVM IR operand from MLIR value for phi instruction.");
        }
        auto llvm_incoming_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(incoming_block);
        auto llvm_incoming_block_operand =
            GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_incoming_block->GetID());
        phi_inst->AddIncoming(llvm_incoming_value, llvm_incoming_block_operand);
        if (llvm_incoming_value->GetInstruction() != nullptr) {
          llvm_incoming_value->GetInstruction()->AddUser(phi_inst);
        }
      }
      auto result = phi_inst->GetResult();
      if (result->IsRegister()) {
        auto llvm_result = static_cast<utils::RegOperand *>(result);
        llvm_result->SetName("phi_result_" + std::to_string(i));
        ++i;
      }
    }

    // After finishing the lowering of the function, we will update the pending copy instructions
    std::queue<utils::BasicBlock *> blocks_to_erase;
    std::unordered_set<inst::Instruction *> copy_insts_to_erase;
    while (!pending_copy_insts_.empty()) {
      auto copy_inst = pending_copy_insts_.front();
      pending_copy_insts_.pop();
      // replace all users with the source operand
      auto src_operand = copy_inst->GetOperands()[0];
      if (src_operand->GetInstruction() != nullptr) {
        if (src_operand->GetInstruction()->VerifyOpKind(inst::OpKind::Copy)) {
          // If the source operand is also a copy instruction, we need to get the original source operand
          auto original_src_operand = src_operand;
          while (original_src_operand->GetInstruction() != nullptr &&
                 original_src_operand->GetInstruction()->VerifyOpKind(inst::OpKind::Copy)) {
            auto copy_instruction = static_cast<inst::CopyInst *>(original_src_operand->GetInstruction());
            original_src_operand = copy_instruction->GetOperands()[0];
          }
          src_operand = original_src_operand;
        }
      }
      copy_inst->ReplaceAllUsersWith(src_operand);
      // Record the block to erase the copy instruction later
      blocks_to_erase.push(copy_inst->GetParentBlock());
      copy_insts_to_erase.insert(copy_inst);
    }
    while (!blocks_to_erase.empty()) {
      auto block = blocks_to_erase.front();
      blocks_to_erase.pop();
      // Erase the copy instruction in the block
      block->RemoveInsts(copy_insts_to_erase);
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
    if (llvm_lhs->GetInstruction() != nullptr) {                                                                  \
      llvm_lhs->GetInstruction()->AddUser(inst);                                                                  \
    }                                                                                                             \
    if (llvm_rhs->GetInstruction() != nullptr) {                                                                  \
      llvm_rhs->GetInstruction()->AddUser(inst);                                                                  \
    }                                                                                                             \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);                                 \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());              \
    inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());                                                 \
  }

// Int
LOWER_BINARY_OP(Add);
LOWER_BINARY_OP(Sub);
LOWER_BINARY_OP(Mul);
LOWER_BINARY_OP(Div);
LOWER_BINARY_OP(Mod);
LOWER_BINARY_OP(Shl);
LOWER_BINARY_OP(LShr);
LOWER_BINARY_OP(AShr);
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
    if (llvm_lhs->GetInstruction() != nullptr) {                                                     \
      llvm_lhs->GetInstruction()->AddUser(inst);                                                     \
    }                                                                                                \
    if (llvm_rhs->GetInstruction() != nullptr) {                                                     \
      llvm_rhs->GetInstruction()->AddUser(inst);                                                     \
    }                                                                                                \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);                    \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult()); \
    inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());                                    \
    return;                                                                                          \
  }

#define FCMP_DISPTACH(OP_NAME, PREDICATE)                                                            \
  if (auto specific_op = mlir::DynamicOrNull<mlir::base::OP_NAME##Op>(op)) {                         \
    auto lhs = specific_op->GetLHS();                                                                \
    auto rhs = specific_op->GetRHS();                                                                \
    auto result = specific_op->GetRes();                                                             \
    auto llvm_lhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(lhs);     \
    auto llvm_rhs = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(rhs);     \
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FCmpInst>(              \
        GET_LLVMIR_CONTEXT(), inst::CmpPredicate::PREDICATE, llvm_lhs, llvm_rhs);                    \
    if (llvm_lhs->GetInstruction() != nullptr) {                                                     \
      llvm_lhs->GetInstruction()->AddUser(inst);                                                     \
    }                                                                                                \
    if (llvm_rhs->GetInstruction() != nullptr) {                                                     \
      llvm_rhs->GetInstruction()->AddUser(inst);                                                     \
    }                                                                                                \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);                    \
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult()); \
    inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());                                    \
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
  FCMP_DISPTACH(EqualF, EQ)
  FCMP_DISPTACH(NotEqualF, NE)
  FCMP_DISPTACH(LessF, LT)
  FCMP_DISPTACH(LessEqualF, LE)
  FCMP_DISPTACH(GreaterF, GT)
  FCMP_DISPTACH(GreaterEqualF, GE)
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerNegFOp(mlir::base::NegFOp *op) -> void {
  // negation can be implemented by subtraction with zero
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(0.0);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::SubFInst>(GET_LLVMIR_CONTEXT(), zero_literal,
                                                                                     llvm_operand);
  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerNotOp(mlir::base::NotOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto true_literal = GET_LLVMIR_CONTEXT()->GetBoolLiteral(true);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::XorInst>(GET_LLVMIR_CONTEXT(), llvm_operand,
                                                                                    true_literal);

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerIntLiteralOp(mlir::base::ConstIntOp *op) -> void {
  /**
   * @brief For integer literal, we will just create a new literal operand in llvm ir
   * and map the mlir value to the llvm ir operand, which will be used in the future instruction lowering
   */
  auto value = op->GetValue();
  auto result = op->GetRes();
  auto int_literal = GET_LLVMIR_CONTEXT()->GetIntLiteral(value);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, int_literal);
}

auto LowerHelper::LowerFloatLiteralOp(mlir::base::ConstFloatOp *op) -> void {
  /**
   * @brief For float literal, we will just create a new literal operand in llvm ir
   * and map the mlir value to the llvm ir operand, which will be used in the future instruction lowering
   */
  auto value = op->GetValue();
  auto result = op->GetRes();
  auto float_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(value);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, float_literal);
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }
  if (zero_literal->GetInstruction() != nullptr) {
    zero_literal->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

/**
 * @brief Float to Bool cast,
 * using Fcmp instruction in llvm ir, comparing with zero
 */
auto LowerHelper::LowerFloat2BoolOp(mlir::base::Float2BoolOp *op) -> void {
  auto operand = op->GetOperand();
  auto result = op->GetRes();
  auto llvm_operand = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(operand);
  auto zero_literal = GET_LLVMIR_CONTEXT()->GetFloatLiteral(0.0);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::FCmpInst>(
      GET_LLVMIR_CONTEXT(), inst::CmpPredicate::NE, llvm_operand, zero_literal);

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }
  if (zero_literal->GetInstruction() != nullptr) {
    zero_literal->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
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

  if (llvm_operand->GetInstruction() != nullptr) {
    llvm_operand->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

/**
 * @brief Lowering function-related operations
 */
auto LowerHelper::LowerFuncCallOp(mlir::base::FuncCallOp *func_call) -> void {
  auto func_name = func_call->GetName();
  auto args = func_call->GetOperands();
  auto result = func_call->GetRes();

  std::vector<llvm::utils::Operand *> llvm_args;
  for (auto *arg : args) {
    auto llvm_arg = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(arg);
    if (llvm_arg == nullptr) {
      printf("Warning: argument %zu of function call %s is not mapped to llvm ir operand, skipping this argument\n",
             arg->num_, func_name.c_str());
      continue;
    }
    llvm_args.emplace_back(llvm_arg);
  }
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::CallInst>(GET_LLVMIR_CONTEXT(), func_name,
                                                                                     llvm_args, result->GetType());

  for (auto llvm_arg : inst->GetArgs()) {
    if (llvm_arg->GetInstruction() != nullptr) {
      llvm_arg->GetInstruction()->AddUser(inst);
    }
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  if (inst->GetResult() != nullptr) {
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  }
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerReturnOp(mlir::func::ReturnOp *return_op) -> void {
  auto ret = return_op->GetReturnValue();
  auto llvm_ret =
      ret == nullptr ? nullptr : GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ret);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::RetInst>(GET_LLVMIR_CONTEXT(), llvm_ret);

  if (llvm_ret != nullptr && llvm_ret->GetInstruction() != nullptr) {
    llvm_ret->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

/**
 * @brief Lowering branch-related operations
 */
auto LowerHelper::LowerBrOp(mlir::base::BrOp *br_op) -> void {
  auto target_block = br_op->GetTarget();
  // Since we lower all basic blocks before lowering instructions
  auto llvm_target_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(target_block);
  if (llvm_target_block == nullptr) {
    throw std::logic_error("Target block not found in LowerBrOp");
  }
  auto llvm_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_target_block->GetID());
  // Create a branch instruction to jump to the target block
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::BrInst>(GET_LLVMIR_CONTEXT(), llvm_label_op);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerCondBrOp(mlir::base::CondBrOp *cond_br_op) -> void {
  auto condition = cond_br_op->GetCondition();
  auto true_block = cond_br_op->GetTrueTarget();
  auto false_block = cond_br_op->GetFalseTarget();

  auto llvm_condition = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(condition);
  auto llvm_true_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(true_block);
  auto llvm_false_block = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIRBlockFromMLIRBlock(false_block);
  if (llvm_condition == nullptr) {
    throw std::logic_error("Condition operand not found in LowerCondBrOp");
  }
  if (llvm_true_block == nullptr) {
    throw std::logic_error("True target block not found in LowerCondBrOp");
  }
  if (llvm_false_block == nullptr) {
    throw std::logic_error("False target block not found in LowerCondBrOp");
  }

  auto llvm_true_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_true_block->GetID());
  auto llvm_false_label_op = GET_LLVMIR_CONTEXT()->GetResourceContext()->GetLabelOperand(llvm_false_block->GetID());

  // Create a conditional branch instruction to jump to the target blocks based on the condition
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::BrCondInst>(
      GET_LLVMIR_CONTEXT(), llvm_condition, llvm_true_label_op, llvm_false_label_op);
  if (llvm_condition->GetInstruction() != nullptr) {
    llvm_condition->GetInstruction()->AddUser(inst);
  }
  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerPhiOp(mlir::base::PhiOp *phi_op) -> void {
  // lower phi instruction
  auto result = phi_op->GetRes();

  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::PhiInst>(
      GET_LLVMIR_CONTEXT(), std::vector<llvm::utils::Operand *>{}, std::vector<llvm::utils::LabelOperand *>{},
      result->GetType());

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());

  // Since the phi instruction needs to be updated with the incoming values and corresponding blocks later,
  // we will store the phi instruction and corresponding mlir phi operation in the pending list, and update
  pending_phi_ops_.push(phi_op);
  pending_phi_insts_.push(inst);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
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

  // Get allocated type which must be basic type
  auto allocated_type = result->GetType();
  while (allocated_type->GetKind() == type::TypeKind::ARRAY) {
    allocated_type = static_cast<type::ArrayType *>(allocated_type)->GetElementType();
  }
  // Get result type, which should be a pointer type pointing to the variable type
  auto result_type = result->GetType();
  if (allocated_type->GetKind() == type::TypeKind::INT) {
    auto &inits = global_var_def->GetIntInitValues();
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
        GET_LLVMIR_CONTEXT(), result_type, allocated_type, var_name, global_var_def->GetDimensions(), inits,
        global_var_def->IsConst());
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddGlobalVarDef(var_name, inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  } else if (allocated_type->GetKind() == type::TypeKind::FLOAT) {
    auto &inits = global_var_def->GetFloatInitValues();
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
        GET_LLVMIR_CONTEXT(), result_type, allocated_type, var_name, global_var_def->GetDimensions(), inits,
        global_var_def->IsConst());
    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddGlobalVarDef(var_name, inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  } else {
    // For other types, it is not supported
    throw std::runtime_error("Unsupported global variable type in LowerGlobalVarDefOp");
  }
}

auto LowerHelper::LowerVarDefOp(mlir::var::VarDefOp *var_def) -> void {
  // Local variable definition will definitely be an alloca instruction
  // For initializations,
  // we will create store instructions to
  // store the initial values to the allocated memory
  auto inits = var_def->GetInits();
  std::vector<llvm::utils::Operand *> llvm_inits;
  for (auto *init : inits) {
    auto llvm_init = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(init);
    llvm_inits.emplace_back(llvm_init);
  }
  /**
   * @brief Initialize method and idea is as follows:
   *  1. If all constants, we will use memcpy to initialize the memory.
   *  2. If not all constants, call memset first, and initialize the memory with store instructions
   */
  bool all_constants = true;
  // If no initialization, we can also directly use memset to initialize
  for (auto llvm_init : llvm_inits) {
    if (llvm_init == nullptr) {
      continue;
    }
    if (!llvm_init->IsLiteral()) {
      all_constants = false;
      break;
    }
  }
  // If empty
  if (llvm_inits.empty()) {
    all_constants = false;
  }
  if (all_constants) {
    // Use memcpy to initialize the memory
    // Get a constant global variable to hold the initialization values

    // It must be a local array, so the allocated type must be an array type
    auto func_name = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetCurrentFunction()->GetFuncName();
    auto const_var_name = func_name + "_" + var_def->GetRes()->name_ + "_init_" + var_def->GetRes()->name_ + "_" +
                          std::to_string(var_def->GetRes()->num_);
    auto var_type = var_def->GetRes()->GetType();
    auto allocated_type = var_type;
    while (allocated_type->GetKind() == type::TypeKind::ARRAY) {
      allocated_type = static_cast<type::ArrayType *>(allocated_type)->GetElementType();
    }
    // Get the var type
    // get the allocated type, which must be basic type
    auto dimensions = var_def->GetDimensions();
    inst::GlobalDefInst *global_def_inst = nullptr;
    if (allocated_type->GetKind() == type::TypeKind::INT) {
      std::vector<int> int_inits = {};
      for (auto llvm_init : llvm_inits) {
        if (llvm_init == nullptr) {
          int_inits.emplace_back(0);
        } else {
          int_inits.emplace_back(static_cast<utils::IntLiteral *>(llvm_init)->GetValue());
        }
      }
      global_def_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
          GET_LLVMIR_CONTEXT(), var_type, allocated_type, const_var_name, var_def->GetDimensions(), int_inits, true);
    } else if (allocated_type->GetKind() == type::TypeKind::FLOAT) {
      std::vector<float> float_inits = {};
      for (auto llvm_init : llvm_inits) {
        if (llvm_init == nullptr) {
          float_inits.emplace_back(0.0F);
        } else {
          float_inits.emplace_back(static_cast<utils::FloatLiteral *>(llvm_init)->GetValue());
        }
      }
      global_def_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GlobalDefInst>(
          GET_LLVMIR_CONTEXT(), var_type, allocated_type, const_var_name, var_def->GetDimensions(), float_inits, true);
    } else {
      auto msg = "Unsupported variable type in LowerVarDefOp for constant initialization, variable name: " +
                 var_def->GetRes()->name_;
      compiler_context_->ReportError(msg);
    }

    GET_LLVMIR_CONTEXT()->GetResourceContext()->AddGlobalVarDef(const_var_name, global_def_inst);
    // Get the pointer to the constant global variable
    auto llvm_const_ptr = global_def_inst->GetResult();
    // Get the pointer to the allocated memory
    auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(var_def->GetRes());
    // Get the total size of the allocated memory, which is the product of the dimensions and the element type size
    auto element_size = var_type->GetDataSize();
    // memcpy(dest, src, size)
    auto call_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::CallInst>(
        GET_LLVMIR_CONTEXT(), "llvm.memcpy.p0.p0.i64",
        std::vector<llvm::utils::Operand *>{llvm_ptr, llvm_const_ptr,
                                            GET_LLVMIR_CONTEXT()->GetLongLiteral(element_size),
                                            GET_LLVMIR_CONTEXT()->GetBoolLiteral(false)},
        GET_LLVMIR_CONTEXT()->GetTypeContext()->GetVoidType());
    if (llvm_ptr->GetInstruction() != nullptr) {
      llvm_ptr->GetInstruction()->AddUser(call_inst);
    }
    if (llvm_const_ptr->GetInstruction() != nullptr) {
      llvm_const_ptr->GetInstruction()->AddUser(call_inst);
    }
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(call_inst);
    call_inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
  } else {
    // Call memset to initialize the memory with zero first, and then create store instructions to initialize the memory
    auto ptr = var_def->GetRes();
    auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
    // Get the total size of the allocated memory, which is the product of the dimensions and the element type size
    auto var_type = var_def->GetRes()->GetType();
    auto element_size = var_type->GetDataSize();
    // memset(ptr, 0, size, false)
    auto call_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::CallInst>(
        GET_LLVMIR_CONTEXT(), "llvm.memset.p0.i64",
        std::vector<llvm::utils::Operand *>{llvm_ptr, GET_LLVMIR_CONTEXT()->GetCharLiteral(0),
                                            GET_LLVMIR_CONTEXT()->GetLongLiteral(element_size),
                                            GET_LLVMIR_CONTEXT()->GetBoolLiteral(false)},
        GET_LLVMIR_CONTEXT()->GetTypeContext()->GetVoidType());
    if (llvm_ptr->GetInstruction() != nullptr) {
      llvm_ptr->GetInstruction()->AddUser(call_inst);
    }
    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(call_inst);
    call_inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());

    // Create store instructions to initialize the memory

    auto dimensions = var_def->GetDimensions();
    // For each initialization value,
    // we will create a store instruction to
    // store the value to the corresponding memory location
    // Calculate the indices for the store instruction
    // based on the dimensions and the position of the initialization value
    for (size_t i = 0; i < llvm_inits.size(); ++i) {
      auto llvm_init = llvm_inits[i];
      // llvm_init == nullptr means default initialization
      if (llvm_init == nullptr) {
        continue;
      }
      std::vector<llvm::utils::Operand *> indices;
      size_t remaining = i;
      for (auto it = dimensions.rbegin(); it != dimensions.rend(); ++it) {
        size_t dim = *it;
        size_t index = remaining % dim;
        remaining /= dim;
        indices.emplace_back(GET_LLVMIR_CONTEXT()->GetIntLiteral(index));
      }
      // Since the allocated type is a pointer type,
      // we need to get the base type to check if it is an array
      auto allocated_type = llvm_ptr->GetType();
      if (allocated_type->GetKind() == type::TypeKind::PTR) {
        allocated_type = static_cast<type::PtrType *>(allocated_type)->GetBaseType();
      }
      // If it is an array, we need to add a zero index to get the pointer to the first element
      // If it is a pointer, we should not add a zero index,
      // since the pointer already points to the first element
      if (allocated_type->GetKind() == type::TypeKind::ARRAY && llvm_ptr->GetInstruction() != nullptr) {
        indices.emplace_back(GET_LLVMIR_CONTEXT()->GetIntLiteral(0));
      }
      // Since the indices are calculated in reverse order, we need to reverse them back
      std::reverse(indices.begin(), indices.end());
      auto result_ptr_type = GET_LLVMIR_CONTEXT()->GetTypeContext()->GetPtrType(llvm_init->GetType());
      auto gep_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GetElementPtrInst>(
          GET_LLVMIR_CONTEXT(), result_ptr_type, llvm_ptr, indices);
      if (llvm_ptr->GetInstruction() != nullptr) {
        llvm_ptr->GetInstruction()->AddUser(gep_inst);
      }
      for (auto llvm_index : indices) {
        if (llvm_index->GetInstruction() != nullptr) {
          llvm_index->GetInstruction()->AddUser(gep_inst);
        }
      }
      auto store_inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::StoreInst>(
          GET_LLVMIR_CONTEXT(), llvm_init, gep_inst->GetResult());

      if (llvm_init->GetInstruction() != nullptr) {
        llvm_init->GetInstruction()->AddUser(store_inst);
      }
      if (gep_inst->GetResult()->GetInstruction() != nullptr) {
        gep_inst->GetResult()->GetInstruction()->AddUser(store_inst);
      }
      GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(gep_inst);
      GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(store_inst);
      gep_inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
      store_inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
    }
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
  auto result = get_ptr_op->GetRes();

  // Preserve getptr-of-getptr boundaries. A nested getptr may be indexing a
  // pointer value rather than descending another aggregate dimension. In
  // particular, strength reduction deliberately represents a related address as
  // `getptr(element_pointer, element_offset)`. Concatenating that offset onto the
  // root array's indices produces invalid LLVM IR once the root GEP has already
  // reached the scalar element type.
  auto ptr = get_ptr_op->GetPtr();
  auto logical_indices = get_ptr_op->GetIndices();

  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);

  // The allocated type is a pointer type,
  // we need to get the base type to check if it is an array
  auto allocated_type = llvm_ptr->GetType();
  if (allocated_type->GetKind() == type::TypeKind::PTR) {
    // It should always be true.
    allocated_type = static_cast<type::PtrType *>(allocated_type)->GetBaseType();
  }

  // If llvm_ptr is a global variable as well as it is not an array
  if (llvm_ptr->IsGlobal() && allocated_type->GetKind() != type::TypeKind::ARRAY) {
    // directly map the result to the global variable operand
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, llvm_ptr);
  } else {
    // Otherwise, get the indices to get the pointer to the specified element
    std::vector<llvm::utils::Operand *> indices;

    // It must be an array
    // Add a zero index to get the pointer to the first element
    // if it is an array
    // Only an alloca/global denotes a whole array object and needs LLVM's leading
    // object index. Parameters, GEP results and phi results are pointer values;
    // even when their logical pointee is an array, their first explicit index is
    // pointer arithmetic and must not be preceded by an inferred zero.
    auto *defining_inst = llvm_ptr->GetInstruction();
    bool is_array_object = defining_inst != nullptr && (defining_inst->VerifyOpKind(inst::OpKind::Alloca) ||
                                                        defining_inst->VerifyOpKind(inst::OpKind::GlobalDef));
    if (allocated_type->GetKind() == type::TypeKind::ARRAY && is_array_object) {
      indices.emplace_back(GET_LLVMIR_CONTEXT()->GetIntLiteral(0));
    }

    for (auto &index : logical_indices) {
      auto llvm_index = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(index);
      indices.emplace_back(llvm_index);
    }
    // Result type is a pointer type pointing to the allocated type
    auto result_type = result->GetType();
    auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::GetElementPtrInst>(
        GET_LLVMIR_CONTEXT(), result_type, llvm_ptr, indices);

    // Use of index
    for (auto llvm_index : indices) {
      if (llvm_index->GetInstruction() != nullptr) {
        llvm_index->GetInstruction()->AddUser(inst);
      }
    }
    // Use of ptr
    if (llvm_ptr->GetInstruction() != nullptr) {
      llvm_ptr->GetInstruction()->AddUser(inst);
    }

    GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
    GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
    inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
  }
}

auto LowerHelper::LowerVarGetValOp(mlir::var::VarGetValOp *get_val_op) -> void {
  auto ptr = get_val_op->GetPtr();
  auto result = get_val_op->GetRes();
  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::LoadInst>(GET_LLVMIR_CONTEXT(),
                                                                                     result->GetType(), llvm_ptr);
  if (llvm_ptr->GetInstruction() != nullptr) {
    llvm_ptr->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(result, inst->GetResult());
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerVarSetOp(mlir::var::VarSetOp *set_val_op) -> void {
  auto ptr = set_val_op->GetPtr();
  auto value = set_val_op->GetNewValue();
  auto llvm_ptr = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(ptr);
  auto llvm_value = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(value);
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::StoreInst>(GET_LLVMIR_CONTEXT(), llvm_value,
                                                                                      llvm_ptr);

  if (llvm_value->GetInstruction() != nullptr) {
    llvm_value->GetInstruction()->AddUser(inst);
  }
  if (llvm_ptr->GetInstruction() != nullptr) {
    llvm_ptr->GetInstruction()->AddUser(inst);
  }

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());
}

// Then lowering copy instruction,
// but it seems to be eliminated in the future.
auto LowerHelper::LowerCopyOp(mlir::base::CopyOp *copy_op) -> void {
  auto src = copy_op->GetSrc();
  auto dst = copy_op->GetRes();
  // Create a pseudo copy instruction,
  // which will be eliminated in the future optimization
  auto llvm_src = GET_LLVMIR_CONTEXT()->GetLowerContext()->GetLLVMIROperandFromMLIRValue(src);
  if (llvm_src == nullptr) {
    throw std::logic_error("Source operand not found in LowerCopyOp");
  }
  auto inst = GET_LLVMIR_CONTEXT()->GetResourceContext()->CreateInst<inst::CopyInst>(GET_LLVMIR_CONTEXT(),
                                                                                     dst->GetType(), llvm_src);

  if (llvm_src->GetInstruction() != nullptr) {
    llvm_src->GetInstruction()->AddUser(inst);
  }
  auto res = inst->GetResult();

  GET_LLVMIR_CONTEXT()->GetLowerContext()->GetInsertPoint()->AddInstBack(inst);
  GET_LLVMIR_CONTEXT()->GetLowerContext()->MapMLIRValueToLLVMIROperand(dst, res);
  inst->SetParentBlock(GET_LLVMIR_CONTEXT()->GetInsertPoint());

  pending_copy_insts_.push(inst);
}
}  // namespace middleend::llvm::lower
