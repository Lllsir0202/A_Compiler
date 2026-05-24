#include "backend/mir/lower/lowerhelper.h"
#include "middleend/llvm/inst/inst.h"

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

  // If we reach here, it means the instruction type is not supported for lowering
  compiler_context_->ReportError("Unsupported instruction type for lowering");
}

#define INTEGER_BINOP_LOWERING(InstType, MIRInstType)                                               \
  auto *lhs = inst->GetLhs();                                                                       \
  auto *rhs = inst->GetRhs();                                                                       \
  auto *mir_lhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(lhs);                           \
  auto *mir_rhs = GET_LOWER_CONTEXT()->GetMIROperandFromLLVMOperand(rhs);                           \
  auto mir_inst = GET_RESOURCE_CONTEXT()->CreateInst<backend::inst::MIRInstType>(mir_lhs, mir_rhs); \
  GET_LOWER_CONTEXT()->MapLLVMOperandToMIROperand(inst->GetResult(), mir_inst->GetResult());

auto LowerHelper::LowerAddInst(middleend::llvm::inst::AddInst *inst) -> void {
  INTEGER_BINOP_LOWERING(AddInst, AddInst)
}

}  // namespace backend::lower