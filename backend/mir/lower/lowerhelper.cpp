#include "backend/mir/lower/lowerhelper.h"
#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/inst/cast/cast.h"
#include "backend/mir/inst/cmp/cmp.h"
#include "backend/mir/inst/func/call.h"
#include "backend/mir/inst/memory/load.h"
#include "backend/mir/inst/memory/store.h"
#include "backend/mir/inst/pseudo/copy.h"
#include "backend/mir/utils/memory/addressinfo.h"
#include "backend/mir/utils/memory/memoryinfo.h"
#include "backend/mir/utils/operand/operand.h"
#include "backend/mir/utils/struct/block.h"
#include "backend/mir/utils/struct/function.h"
#include "middleend/llvm/inst/arithm/arithm.h"
#include "middleend/llvm/inst/cmp/cmp.h"
#include "middleend/llvm/inst/inst.h"
#include "middleend/llvm/inst/memory/globaldef.h"
#include "middleend/llvm/inst/opkind.h"
#include "middleend/llvm/utils/struct/block.h"
#include "type/type.h"

#define GET_MIR_CONTEXT() compiler_context_->GetMIRContext()
#define GET_RESOURCE_CONTEXT() compiler_context_->GetMIRContext()->GetResourceContext()
#define GET_LOWER_CONTEXT() compiler_context_->GetMIRContext()->GetLowerContext()
namespace backend::lower {

#define DISPATCH_LOWERING(InstType)                                                \
  if (auto *casted_inst = dynamic_cast<middleend::llvm::inst::InstType *>(inst)) { \
    Lower##InstType(casted_inst);                                                  \
    return;                                                                        \
  }

// For globaldef, funcdef, funcdeclare, we will lower them in advance
auto LowerHelper::LowerDispatch(middleend::llvm::inst::Instruction *inst) -> void {
  try {
    DISPATCH_LOWERING(AddInst)
    DISPATCH_LOWERING(SubInst)
    DISPATCH_LOWERING(MulInst)
    DISPATCH_LOWERING(DivInst)
    DISPATCH_LOWERING(ModInst)
    DISPATCH_LOWERING(AddFInst)
    DISPATCH_LOWERING(SubFInst)
    DISPATCH_LOWERING(MulFInst)
    DISPATCH_LOWERING(DivFInst)
    DISPATCH_LOWERING(ICmpInst)
    DISPATCH_LOWERING(FCmpInst)
    DISPATCH_LOWERING(XorInst)
    DISPATCH_LOWERING(ZextInst)
    DISPATCH_LOWERING(SitoFpInst)
    DISPATCH_LOWERING(FptoSiInst)
    DISPATCH_LOWERING(UitoFpInst)
    DISPATCH_LOWERING(CallInst)
    DISPATCH_LOWERING(RetInst)
    DISPATCH_LOWERING(BrInst)
    DISPATCH_LOWERING(BrCondInst)
    DISPATCH_LOWERING(PhiInst)
    DISPATCH_LOWERING(AllocaInst)
    DISPATCH_LOWERING(LoadInst)
    DISPATCH_LOWERING(StoreInst)
    DISPATCH_LOWERING(GetElementPtrInst)
  } catch (std::exception &e) {
    compiler_context_->ReportError("Error during instruction lowering: " + std::string(e.what()));
    return;
  }
  // If we reach here, it means the instruction type is not supported for lowering
  compiler_context_->ReportError("Unsupported instruction type for lowering");
}

auto LowerHelper::LowerBlock(middleend::llvm::utils::BasicBlock *bb) -> utils::Block * {
  auto block = GET_RESOURCE_CONTEXT()->CreateBlock(bb->GetID());
  // Add the block to the current function
  GET_LOWER_CONTEXT()->GetCurrentFunction()->AddBlock(block);
  GET_LOWER_CONTEXT()->GetCurrentFunction()->AddBlockById(block->GetId(), block);
  GET_LOWER_CONTEXT()->MapBlockToBasicBlock(block, bb);
  // Map
  return block;
}

/**
 * @brief Lower specific llvmir function definition instruction
 */

auto LowerHelper::LowerFunction(middleend::llvm::utils::Function *function) -> void {
  try {
    auto mir_function =
        GET_MIR_CONTEXT()->GetResourceContext()->CreateFunction(function->GetFuncName(), function->GetReturnType());

    // Add the function to the MIR context
    GET_LOWER_CONTEXT()->SetCurrentFunction(mir_function);
    for (auto basicblock : function->GetBasicBlocks()) {
      auto block = LowerBlock(basicblock);
      GET_MIR_CONTEXT()->GetLowerContext()->SetInsertPoint(block);
      for (auto inst : basicblock->GetInstructions()) {
        LowerDispatch(inst);
      }
    }

    // Finish lowering a function, deal with the phi instructions here
    while (!pending_phi_mirs_.empty()) {
      auto phi_mir = pending_phi_mirs_.front();
      pending_phi_mirs_.pop();
      auto phi_llvm = pending_phi_llvmirs_.front();
      pending_phi_llvmirs_.pop();

      // Map from phi_llvm to phi_mir
      auto incoming_values = phi_llvm->GetIncomingValues();
      auto incoming_blocks = phi_llvm->GetIncomingBlocks();
      for (size_t i = 0; i < phi_llvm->GetIncomingSize(); ++i) {
        auto incoming_value = incoming_values[i];
        auto incoming_block = incoming_blocks[i];
        auto mir_incoming_value = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(incoming_value);
        auto mir_incoming_block = GET_RESOURCE_CONTEXT()->GetLabelOperand(incoming_block->GetID());
        phi_mir->AddIncoming(mir_incoming_value, mir_incoming_block);
      }
    }

  } catch (std::exception &e) {
    compiler_context_->ReportError("Error during function lowering: " + std::string(e.what()));
    return;
  }
}

/**
 * @brief Since a lot of instructions have really similar lowering logic,
 * and most of the lowered instructions are just as the llvmir instructions.
 * Thus we can just directly lower the llvmir instructions to mir instructions without much modification,
 * and using a macro or a template function might be enough.
 */
#define LOWER_BINARY_INST(InstType)                                                                                 \
  auto LowerHelper::Lower##InstType(middleend::llvm::inst::InstType *inst)->void {                                  \
    auto lhs = inst->GetLHS();                                                                                      \
    auto rhs = inst->GetRHS();                                                                                      \
    auto result = inst->GetResult();                                                                                \
    auto mir_lhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(lhs);                                          \
    auto mir_rhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(rhs);                                          \
                                                                                                                    \
    /* Create a new MIR instruction for the binary operation, and add it to the current basic block*/               \
    auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::InstType>(GET_MIR_CONTEXT(), mir_lhs, mir_rhs); \
    auto mir_result = mir_inst->GetResult();                                                                        \
    GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);                                            \
    if (mir_lhs->GetDefInst() != nullptr) {                                                                         \
      mir_lhs->GetDefInst()->AddUser(mir_inst);                                                                     \
    }                                                                                                               \
    if (mir_rhs->GetDefInst() != nullptr) {                                                                         \
      mir_rhs->GetDefInst()->AddUser(mir_inst);                                                                     \
    }                                                                                                               \
    mir_result->SetType(result->GetType());                                                                         \
    GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);                                                   \
    mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());                                                \
  }

LOWER_BINARY_INST(AddInst)
LOWER_BINARY_INST(AddFInst)
LOWER_BINARY_INST(SubInst)
LOWER_BINARY_INST(SubFInst)
LOWER_BINARY_INST(MulInst)
LOWER_BINARY_INST(MulFInst)
LOWER_BINARY_INST(DivInst)
LOWER_BINARY_INST(DivFInst)
LOWER_BINARY_INST(ModInst)

auto LowerHelper::LowerICmpInst(middleend::llvm::inst::ICmpInst *icmp_inst) -> void {
  auto predicate = inst::CmpPredicateFromLLVM(icmp_inst->GetPredicate());
  auto lhs = icmp_inst->GetLHS();
  auto rhs = icmp_inst->GetRHS();
  auto result = icmp_inst->GetResult();

  auto mir_lhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(lhs);
  auto mir_rhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(rhs);

  // Create a new MIR instruction for the integer comparison operation, and add it to the current basic block
  auto mir_inst =
      GET_RESOURCE_CONTEXT()->CreateInstruction<inst::ICmpInst>(GET_MIR_CONTEXT(), mir_lhs, mir_rhs, predicate);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  mir_result->SetType(result->GetType());
  if (mir_lhs->GetDefInst() != nullptr) {
    mir_lhs->GetDefInst()->AddUser(mir_inst);
  }
  if (mir_rhs->GetDefInst() != nullptr) {
    mir_rhs->GetDefInst()->AddUser(mir_inst);
  }
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerFCmpInst(middleend::llvm::inst::FCmpInst *fcmp_inst) -> void {
  auto predicate = inst::CmpPredicateFromLLVM(fcmp_inst->GetPredicate());
  auto lhs = fcmp_inst->GetLHS();
  auto rhs = fcmp_inst->GetRHS();
  auto result = fcmp_inst->GetResult();

  auto mir_lhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(lhs);
  auto mir_rhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(rhs);

  // Create a new MIR instruction for the floating-point comparison operation, and add it to the current basic block
  auto mir_inst =
      GET_RESOURCE_CONTEXT()->CreateInstruction<inst::FCmpInst>(GET_MIR_CONTEXT(), mir_lhs, mir_rhs, predicate);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  mir_result->SetType(result->GetType());
  if (mir_lhs->GetDefInst() != nullptr) {
    mir_lhs->GetDefInst()->AddUser(mir_inst);
  }
  if (mir_rhs->GetDefInst() != nullptr) {
    mir_rhs->GetDefInst()->AddUser(mir_inst);
  }
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerXorInst(middleend::llvm::inst::XorInst *xor_inst) -> void {
  auto lhs = xor_inst->GetLHS();
  auto rhs = xor_inst->GetRHS();
  auto result = xor_inst->GetResult();
  auto mir_lhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(lhs);
  auto mir_rhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(rhs);

  // Create a new MIR instruction for the XOR operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::XorInst>(GET_MIR_CONTEXT(), mir_lhs, mir_rhs);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_lhs->GetDefInst() != nullptr) {
    mir_lhs->GetDefInst()->AddUser(mir_inst);
  }
  if (mir_rhs->GetDefInst() != nullptr) {
    mir_rhs->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

// Cast Insts
auto LowerHelper::LowerZextInst(middleend::llvm::inst::ZextInst *zext_inst) -> void {
  auto operand = zext_inst->GetSrc();
  auto result = zext_inst->GetResult();
  auto mir_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(operand);

  // Create a new MIR instruction for the zero extension operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::ZextInst>(GET_MIR_CONTEXT(), mir_operand);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_operand->GetDefInst() != nullptr) {
    mir_operand->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerSitoFpInst(middleend::llvm::inst::SitoFpInst *sitofp_inst) -> void {
  auto operand = sitofp_inst->GetSrc();
  auto result = sitofp_inst->GetResult();
  auto mir_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(operand);

  // Create a new MIR instruction for the signed integer to floating-point conversion operation, and add it to the
  // current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::Int2FloatInst>(GET_MIR_CONTEXT(), mir_operand);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_operand->GetDefInst() != nullptr) {
    mir_operand->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerFptoSiInst(middleend::llvm::inst::FptoSiInst *fptosi_inst) -> void {
  auto operand = fptosi_inst->GetSrc();
  auto result = fptosi_inst->GetResult();
  auto mir_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(operand);

  // Create a new MIR instruction for the floating-point to signed integer conversion operation, and add it to the
  // current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::Float2IntInst>(GET_MIR_CONTEXT(), mir_operand);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_operand->GetDefInst() != nullptr) {
    mir_operand->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerUitoFpInst(middleend::llvm::inst::UitoFpInst *uitofp_inst) -> void {
  auto operand = uitofp_inst->GetSrc();
  auto result = uitofp_inst->GetResult();
  auto mir_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(operand);

  // Create a new MIR instruction for the unsigned integer to floating-point conversion operation, and add it to the
  // current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::Bool2FloatInst>(GET_MIR_CONTEXT(), mir_operand);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_operand->GetDefInst() != nullptr) {
    mir_operand->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

// Important: function-related insts
auto LowerHelper::LowerCallInst(middleend::llvm::inst::CallInst *call_inst) -> void {
  auto callee = call_inst->GetCallee();
  auto args = call_inst->GetArgs();
  std::vector<utils::Operand *> mir_args;
  for (size_t i = 0; i < args.size(); ++i) {
    auto arg = args[i];
    auto mir_arg = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(arg);

    mir_arg->SetIsParameter(true);
    mir_arg->SetIndex(i);
    mir_args.emplace_back(mir_arg);
  }
  // Create a new MIR instruction for the function call operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::CallInst>(GET_MIR_CONTEXT(), callee,
                                                                            call_inst->GetReturnType(), mir_args);
  auto result = call_inst->GetResult();
  if (result != nullptr) {
    auto mir_result = mir_inst->GetResult();
    GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
    if (mir_result != nullptr) {
      mir_result->SetIsReturnValue(true);
      mir_result->SetType(result->GetType());
    }
  }
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerRetInst(middleend::llvm::inst::RetInst *return_inst) -> void {
  auto ret_value = return_inst->GetRetValue();
  auto mir_ret_value = ret_value == nullptr ? nullptr : GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(ret_value);

  // Create a new MIR instruction for the return operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::RetInst>(GET_MIR_CONTEXT(), mir_ret_value);
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerBrInst(middleend::llvm::inst::BrInst *br_inst) -> void {
  auto target = br_inst->GetTargetBlockID();
  auto target_label_op = GET_RESOURCE_CONTEXT()->GetLabelOperand(target);
  // Create a new MIR instruction for the unconditional branch operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::BrInst>(GET_MIR_CONTEXT(), target_label_op);
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerBrCondInst(middleend::llvm::inst::BrCondInst *cond_br_inst) -> void {
  auto condition = cond_br_inst->GetCondition();
  auto true_target = cond_br_inst->GetTrueTargetBlockID();
  auto false_target = cond_br_inst->GetFalseTargetBlockID();

  auto mir_condition = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(condition);
  auto true_label_op = GET_RESOURCE_CONTEXT()->GetLabelOperand(true_target);
  auto false_label_op = GET_RESOURCE_CONTEXT()->GetLabelOperand(false_target);

  // Create a new MIR instruction for the conditional branch operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::BrCondInst>(GET_MIR_CONTEXT(), mir_condition,
                                                                              true_label_op, false_label_op);
  if (mir_condition->GetDefInst() != nullptr) {
    mir_condition->GetDefInst()->AddUser(mir_inst);
  }
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerPhiInst(middleend::llvm::inst::PhiInst *phi_inst) -> void {
  auto result = phi_inst->GetResult();
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::PhiInst>(GET_MIR_CONTEXT(), result->GetType());
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_inst->GetResult());
  if (mir_inst->GetResult()->GetDefInst() != nullptr) {
    mir_inst->GetResult()->GetDefInst()->AddUser(mir_inst);
  }
  // We cannot set the incoming values and blocks until we finish lowering the current function,
  // because the incoming blocks might not be created yet. Thus we will store the phi instructions in a pending list,
  // and handle them after we finish lowering the current function.
  pending_phi_mirs_.push(mir_inst);
  pending_phi_llvmirs_.push(phi_inst);
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

// Deal with the alloca, load and store instructions, which are related to memory operations
auto LowerHelper::LowerAllocaInst(middleend::llvm::inst::AllocaInst *alloca_inst) -> void {
  // Change alloca to frame operand
  auto frameoperand = GET_MIR_CONTEXT()->GetFrameOperand();
  auto current_function = GET_LOWER_CONTEXT()->GetCurrentFunction();

  // Should calculate the size and align of the allocated type.
  auto allocated_type = alloca_inst->GetAllocatedType();
  auto result_type = alloca_inst->GetResult()->GetType();
  // Result type must be a pointer.
  if (result_type->GetKind() == type::TypeKind::PTR) {
    result_type = static_cast<type::PtrType *>(result_type)->GetBaseType();
  }

  // For frameoperand, the type will be the type of the result type, instead of
  // the pointer to the variable type.
  frameoperand->SetType(result_type);
  auto size = result_type->GetDataSize();
  // Then create size and align
  auto align = allocated_type->GetAlign();
  auto frame_info = current_function->AddFrameInfo(size, align);
  GET_LOWER_CONTEXT()->MapFrameOperandToFrameInfo(frameoperand, frame_info);
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(alloca_inst->GetResult(), frameoperand);
}

auto LowerHelper::LowerLoadInst(middleend::llvm::inst::LoadInst *load_inst) -> void {
  auto ptr_operand = load_inst->GetPtr();
  auto result = load_inst->GetResult();
  auto mir_ptr_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(ptr_operand);

  // Create a new MIR instruction for the load operation, and add it to the current basic block
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInstruction<inst::LoadInst>(GET_MIR_CONTEXT(), mir_ptr_operand);
  auto mir_result = mir_inst->GetResult();
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(result, mir_result);
  if (mir_ptr_operand->GetDefInst() != nullptr) {
    mir_ptr_operand->GetDefInst()->AddUser(mir_inst);
  }
  mir_result->SetType(result->GetType());
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerStoreInst(middleend::llvm::inst::StoreInst *store_inst) -> void {
  auto value_operand = store_inst->GetValue();
  auto ptr_operand = store_inst->GetPtr();
  auto mir_value_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(value_operand);
  auto mir_ptr_operand = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(ptr_operand);

  // Create a new MIR instruction for the store operation, and add it to the current basic block
  auto mir_inst =
      GET_RESOURCE_CONTEXT()->CreateInstruction<inst::StoreInst>(GET_MIR_CONTEXT(), mir_value_operand, mir_ptr_operand);
  if (mir_value_operand->GetDefInst() != nullptr) {
    mir_value_operand->GetDefInst()->AddUser(mir_inst);
  }
  if (mir_ptr_operand->GetDefInst() != nullptr) {
    mir_ptr_operand->GetDefInst()->AddUser(mir_inst);
  }
  GET_LOWER_CONTEXT()->GetInsertPoint()->AddInstBack(mir_inst);
  mir_inst->SetParentBlock(GET_LOWER_CONTEXT()->GetInsertPoint());
}

auto LowerHelper::LowerGetElementPtrInst(middleend::llvm::inst::GetElementPtrInst *gep_inst) -> void {
  /**
   * @brief For getelementptr instruction, we need to decay it to lower level information
   * We need to record
   * 1. the base pointer,
   * 2. the offset(will be handled later),
   * 3. the indices
   * 4. the dimensions of the original variable.
   */

  /**
   * @note
   * For getelementptr instruction, the base pointer can be:
   * 1. global variable definition
   * 2. local variable
   * 3. function argument
   */
  auto base_ptr = gep_inst->GetBasePtr();
  auto indices = gep_inst->GetIndices();

  std::vector<int> dimensions;
  if (base_ptr->GetInstruction() != nullptr) {
    auto inst = base_ptr->GetInstruction();
    if (inst->VerifyOpKind(middleend::llvm::inst::OpKind::Alloca)) {
      auto alloca_inst = static_cast<middleend::llvm::inst::AllocaInst *>(inst);
      // Means the base pointer is from a local variable
      dimensions = alloca_inst->GetDimensions();
    } else if (inst->VerifyOpKind(middleend::llvm::inst::OpKind::GlobalDef)) {
      auto global_def_inst = static_cast<middleend::llvm::inst::GlobalDefInst *>(inst);
      dimensions = global_def_inst->GetDimensions();
    }
  } else {
    // Means the base pointer is from a function parameter
    auto base_ptr_type = base_ptr->GetType();
    type::Type *base_type;
    if (base_ptr_type->GetKind() == type::TypeKind::PTR) {
      base_type = static_cast<type::PtrType *>(base_ptr_type)->GetBaseType();
    } else {
      // Should not happen, just in case.
      base_type = base_ptr_type;
    }

    // Get the dimensions
    // For function parameter, the first dimension is always -1,
    // since it is represented as a pointer to other dimensions' array.
    dimensions.emplace_back(-1);
    while (base_type->GetKind() == type::TypeKind::ARRAY) {
      auto array_type = static_cast<type::ArrayType *>(base_type);
      dimensions.emplace_back(array_type->GetSize());
      base_type = array_type->GetElementType();
    }

    // Finish getting the dimensions
  }

  // Record all the information and create the memoryoperand
  auto memory_operand = GET_RESOURCE_CONTEXT()->GetNewMemoryOperand();

  // Address information will be used to get access to the element in the future.
  auto address_info = utils::AddressInfo();

  auto mir_base_ptr = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(base_ptr);

  auto mir_indices = std::vector<utils::Operand *>();
  for (auto index : indices) {
    auto mir_index = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(index);
    mir_indices.emplace_back(mir_index);
  }

  address_info.base_ptr_ = mir_base_ptr;
  address_info.dimensions_ = dimensions;
  address_info.indices_ = mir_indices;
  memory_operand->SetAddressInfo(address_info);

  // Memory information will be used as a record of the result's memory layout
  auto memory_info = utils::MemoryInfo();
  memory_info.align_ = gep_inst->GetResult()->GetType()->GetAlign();
  memory_info.size_ = gep_inst->GetResult()->GetType()->GetDataSize();
  memory_operand->SetMemoryInfo(memory_info);

  // Then map the memory operand to the original llvm result operand.
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(gep_inst->GetResult(), memory_operand);
}

#undef DISPATCH_LOWERING
#undef LOWER_BINARY_INST

}  // namespace backend::lower