#include "backend/mir/lower/lowerhelper.h"
#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/inst/cast/cast.h"
#include "backend/mir/inst/cmp/cmp.h"
#include "backend/mir/inst/func/call.h"
#include "backend/mir/utils/struct/block.h"
#include "middleend/llvm/inst/arithm/arithm.h"
#include "middleend/llvm/inst/inst.h"
#include "mir/utils/operand/operand.h"

#define GET_MIR_CONTEXT() compiler_context_->GetMIRContext()
#define GET_RESOURCE_CONTEXT() compiler_context_->GetMIRContext()->GetResourceContext()
#define GET_LOWER_CONTEXT() compiler_context_->GetMIRContext()->GetLowerContext()
namespace backend::lower {

#define DISPATCH_LOWERING(InstType)                                                \
  if (auto *casted_inst = dynamic_cast<middleend::llvm::inst::InstType *>(inst)) { \
    Lower##InstType(casted_inst);                                                  \
    return;                                                                        \
  }

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
    DISPATCH_LOWERING(ZextInst)
    DISPATCH_LOWERING(SitoFpInst)
    DISPATCH_LOWERING(FptoSiInst)
    DISPATCH_LOWERING(UitoFpInst)
    DISPATCH_LOWERING(CallInst)
    DISPATCH_LOWERING(FuncDefInst)
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

}  // namespace backend::lower