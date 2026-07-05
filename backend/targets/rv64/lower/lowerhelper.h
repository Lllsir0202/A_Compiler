#ifndef BACKEND_TARGETS_RV64_LOWER_LOWERHELPER_H
#define BACKEND_TARGETS_RV64_LOWER_LOWERHELPER_H

#include "backend/mir/inst/arithm/arithm.h"
#include "backend/mir/inst/branch/branch.h"
#include "backend/mir/inst/branch/ret.h"
#include "backend/mir/inst/cast/cast.h"
#include "backend/mir/inst/cmp/cmp.h"
#include "backend/mir/inst/func/call.h"
#include "backend/mir/inst/func/funcdeclare.h"
#include "backend/mir/inst/func/funcdef.h"
#include "backend/mir/inst/inst.h"
#include "backend/mir/inst/memory/globaldef.h"
#include "backend/mir/inst/memory/load.h"
#include "backend/mir/inst/memory/store.h"
#include "backend/mir/inst/pseudo/copy.h"
#include "backend/mir/inst/pseudo/phi.h"
#include "backend/targets/rv64/getter/getter.h"
#include "backend/targets/utils/operand/targetoperand.h"
namespace backend::targetir::rv64 {
class LowerHelper {
 public:
  auto LowerDispatch(backend::inst::Instruction *inst) -> void;

 private:
  // Contructor
  explicit LowerHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  context::CompilerContext *compiler_context_;
  RV64Getter rv64_getter_;
  friend class RV64Lower;

  auto EnsureRegisterOperand(backend::utils::Operand *mir_operand,
                             const char *operand_name) -> backend::targetir::TargetOperand *;

  auto LowerAdd(backend::inst::AddInst *inst) -> void;
  auto LowerSub(backend::inst::SubInst *inst) -> void;
  auto LowerMul(backend::inst::MulInst *inst) -> void;
  auto LowerDiv(backend::inst::DivInst *inst) -> void;
  auto LowerMod(backend::inst::ModInst *inst) -> void;

  auto LowerAddF(backend::inst::AddFInst *inst) -> void;
  auto LowerSubF(backend::inst::SubFInst *inst) -> void;
  auto LowerMulF(backend::inst::MulFInst *inst) -> void;
  auto LowerDivF(backend::inst::DivFInst *inst) -> void;

  auto LowerICmp(backend::inst::ICmpInst *inst) -> void;
  auto LowerFCmp(backend::inst::FCmpInst *inst) -> void;
  auto LowerXor(backend::inst::XorInst *inst) -> void;

  auto LowerLoad(backend::inst::LoadInst *inst) -> void;
  auto LowerStore(backend::inst::StoreInst *inst) -> void;
  auto LowerGlobalDef(backend::inst::GlobalDefInst *inst) -> void;

  auto LowerBr(backend::inst::BrInst *inst) -> void;
  auto LowerCondBr(backend::inst::BrCondInst *inst) -> void;
  auto LowerRet(backend::inst::RetInst *inst) -> void;

  auto LowerFuncDef(backend::inst::FuncDefInst *inst) -> void;
  auto LowerFuncDeclare(backend::inst::FuncDeclareInst *inst) -> void;
  auto LowerCall(backend::inst::CallInst *inst) -> void;

  auto LowerZext(backend::inst::ZextInst *inst) -> void;
  auto LowerInt2Float(backend::inst::Int2FloatInst *inst) -> void;
  auto LowerFloat2Int(backend::inst::Float2IntInst *inst) -> void;
  auto LowerBool2Float(backend::inst::Bool2FloatInst *inst) -> void;

  auto LowerCopy(backend::inst::CopyInst *inst) -> void;
  auto LowerPhi(backend::inst::PhiInst *inst) -> void;
};

}  // namespace backend::targetir::rv64

#endif
