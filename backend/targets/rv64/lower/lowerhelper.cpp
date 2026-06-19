#include "backend/targets/rv64/lower/lowerhelper.h"

#include <stdexcept>
#include <string>
#include "mir/inst/inst.h"

namespace backend::targetir::rv64 {

auto LowerHelper::LowerDispatch(backend::inst::Instruction *inst) -> void {
  if (inst == nullptr) {
    throw std::invalid_argument("RV64 lower received a null MIR instruction");
  }

#define DISPATCH_MIR_INST(MIR_INST, LOWER_METHOD)           \
  if (auto *casted = inst::DynamicOrNull<MIR_INST>(inst)) { \
    LOWER_METHOD(casted);                                   \
    return;                                                 \
  }

  DISPATCH_MIR_INST(backend::inst::AddInst, LowerAdd)
  DISPATCH_MIR_INST(backend::inst::SubInst, LowerSub)
  DISPATCH_MIR_INST(backend::inst::MulInst, LowerMul)
  DISPATCH_MIR_INST(backend::inst::DivInst, LowerDiv)
  DISPATCH_MIR_INST(backend::inst::ModInst, LowerMod)

  DISPATCH_MIR_INST(backend::inst::AddFInst, LowerAddF)
  DISPATCH_MIR_INST(backend::inst::SubFInst, LowerSubF)
  DISPATCH_MIR_INST(backend::inst::MulFInst, LowerMulF)
  DISPATCH_MIR_INST(backend::inst::DivFInst, LowerDivF)

  DISPATCH_MIR_INST(backend::inst::ICmpInst, LowerICmp)
  DISPATCH_MIR_INST(backend::inst::FCmpInst, LowerFCmp)
  DISPATCH_MIR_INST(backend::inst::XorInst, LowerXor)

  DISPATCH_MIR_INST(backend::inst::LoadInst, LowerLoad)
  DISPATCH_MIR_INST(backend::inst::StoreInst, LowerStore)
  DISPATCH_MIR_INST(backend::inst::GlobalDefInst, LowerGlobalDef)

  DISPATCH_MIR_INST(backend::inst::BrInst, LowerBr)
  DISPATCH_MIR_INST(backend::inst::BrCondInst, LowerCondBr)
  DISPATCH_MIR_INST(backend::inst::RetInst, LowerRet)

  DISPATCH_MIR_INST(backend::inst::FuncDefInst, LowerFuncDef)
  DISPATCH_MIR_INST(backend::inst::FuncDeclareInst, LowerFuncDeclare)
  DISPATCH_MIR_INST(backend::inst::CallInst, LowerCall)

  DISPATCH_MIR_INST(backend::inst::ZextInst, LowerZext)
  DISPATCH_MIR_INST(backend::inst::Int2FloatInst, LowerInt2Float)
  DISPATCH_MIR_INST(backend::inst::Float2IntInst, LowerFloat2Int)
  DISPATCH_MIR_INST(backend::inst::Bool2FloatInst, LowerBool2Float)

  DISPATCH_MIR_INST(backend::inst::CopyInst, LowerCopy)
  DISPATCH_MIR_INST(backend::inst::PhiInst, LowerPhi)

#undef DISPATCH_MIR_INST
  throw std::runtime_error("Unsupported MIR instruction in RV64 lower dispatch");
}

auto LowerHelper::LowerAdd([[maybe_unused]] backend::inst::AddInst *inst) -> void {}
auto LowerHelper::LowerSub([[maybe_unused]] backend::inst::SubInst *inst) -> void {}
auto LowerHelper::LowerMul([[maybe_unused]] backend::inst::MulInst *inst) -> void {}
auto LowerHelper::LowerDiv([[maybe_unused]] backend::inst::DivInst *inst) -> void {}
auto LowerHelper::LowerMod([[maybe_unused]] backend::inst::ModInst *inst) -> void {}

auto LowerHelper::LowerAddF([[maybe_unused]] backend::inst::AddFInst *inst) -> void {}
auto LowerHelper::LowerSubF([[maybe_unused]] backend::inst::SubFInst *inst) -> void {}
auto LowerHelper::LowerMulF([[maybe_unused]] backend::inst::MulFInst *inst) -> void {}
auto LowerHelper::LowerDivF([[maybe_unused]] backend::inst::DivFInst *inst) -> void {}

auto LowerHelper::LowerICmp([[maybe_unused]] backend::inst::ICmpInst *inst) -> void {}
auto LowerHelper::LowerFCmp([[maybe_unused]] backend::inst::FCmpInst *inst) -> void {}
auto LowerHelper::LowerXor([[maybe_unused]] backend::inst::XorInst *inst) -> void {}

auto LowerHelper::LowerLoad([[maybe_unused]] backend::inst::LoadInst *inst) -> void {}
auto LowerHelper::LowerStore([[maybe_unused]] backend::inst::StoreInst *inst) -> void {}
auto LowerHelper::LowerGlobalDef([[maybe_unused]] backend::inst::GlobalDefInst *inst) -> void {}

auto LowerHelper::LowerBr([[maybe_unused]] backend::inst::BrInst *inst) -> void {}
auto LowerHelper::LowerCondBr([[maybe_unused]] backend::inst::BrCondInst *inst) -> void {}
auto LowerHelper::LowerRet([[maybe_unused]] backend::inst::RetInst *inst) -> void {}

auto LowerHelper::LowerFuncDef([[maybe_unused]] backend::inst::FuncDefInst *inst) -> void {}
auto LowerHelper::LowerFuncDeclare([[maybe_unused]] backend::inst::FuncDeclareInst *inst) -> void {}
auto LowerHelper::LowerCall([[maybe_unused]] backend::inst::CallInst *inst) -> void {}

auto LowerHelper::LowerZext([[maybe_unused]] backend::inst::ZextInst *inst) -> void {}
auto LowerHelper::LowerInt2Float([[maybe_unused]] backend::inst::Int2FloatInst *inst) -> void {}
auto LowerHelper::LowerFloat2Int([[maybe_unused]] backend::inst::Float2IntInst *inst) -> void {}
auto LowerHelper::LowerBool2Float([[maybe_unused]] backend::inst::Bool2FloatInst *inst) -> void {}

auto LowerHelper::LowerCopy([[maybe_unused]] backend::inst::CopyInst *inst) -> void {}
auto LowerHelper::LowerPhi([[maybe_unused]] backend::inst::PhiInst *inst) -> void {}

}  // namespace backend::targetir::rv64
