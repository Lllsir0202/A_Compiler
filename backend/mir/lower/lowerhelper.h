#ifndef BACKEND_MIR_LOWER_LOWERHELPER_H
#define BACKEND_MIR_LOWER_LOWERHELPER_H

#include <queue>
#include <utility>
#include <vector>
#include "backend/mir/inst/pseudo/phi.h"
#include "compiler_context.h"
#include "llvm/utils/struct/block.h"
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
#include "middleend/llvm/inst/memory/load.h"
#include "middleend/llvm/inst/memory/store.h"
#include "mir/utils/struct/block.h"

namespace backend::lower {
class LowerHelper {
 public:
  explicit LowerHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}

  auto LowerFunction(middleend::llvm::utils::Function *function) -> void;

 private:
  context::CompilerContext *compiler_context_;
  auto LowerDispatch(middleend::llvm::inst::Instruction *inst) -> void;

  // Lower Blocks
  auto LowerBlock(middleend::llvm::utils::BasicBlock *bb) -> utils::Block *;

  // Emit the phi copies for one CFG edge as a *parallel* copy: the copies are
  // sequentialized so simultaneous updates (e.g. a swap a<->b at a loop
  // back-edge) are not corrupted by the lost-copy problem, breaking cycles with
  // a fresh temporary. Inserted before the terminator of `block`.
  auto EmitParallelCopy(utils::Block *block, std::vector<std::pair<utils::Operand *, utils::Operand *>> copies) -> void;

  // Lowering llvmir instructions
  // Integer
  auto LowerAddInst(middleend::llvm::inst::AddInst *add_inst) -> void;
  auto LowerSubInst(middleend::llvm::inst::SubInst *sub_inst) -> void;
  auto LowerMulInst(middleend::llvm::inst::MulInst *mul_inst) -> void;
  auto LowerDivInst(middleend::llvm::inst::DivInst *div_inst) -> void;
  auto LowerModInst(middleend::llvm::inst::ModInst *mod_inst) -> void;
  auto LowerShlInst(middleend::llvm::inst::ShlInst *shl_inst) -> void;
  auto LowerLShrInst(middleend::llvm::inst::LShrInst *lshr_inst) -> void;
  auto LowerAShrInst(middleend::llvm::inst::AShrInst *ashr_inst) -> void;

  // Float
  auto LowerAddFInst(middleend::llvm::inst::AddFInst *add_f_inst) -> void;
  auto LowerSubFInst(middleend::llvm::inst::SubFInst *sub_f_inst) -> void;
  auto LowerMulFInst(middleend::llvm::inst::MulFInst *mul_f_inst) -> void;
  auto LowerDivFInst(middleend::llvm::inst::DivFInst *div_f_inst) -> void;

  // Comparison instructions
  // Int
  auto LowerICmpInst(middleend::llvm::inst::ICmpInst *icmp_inst) -> void;
  // Float
  auto LowerFCmpInst(middleend::llvm::inst::FCmpInst *fcmp_inst) -> void;
  auto LowerXorInst(middleend::llvm::inst::XorInst *xor_inst) -> void;

  // Cast instructions
  auto LowerZextInst(middleend::llvm::inst::ZextInst *zext_inst) -> void;
  auto LowerSextInst(middleend::llvm::inst::SextInst *sext_inst) -> void;
  auto LowerTruncInst(middleend::llvm::inst::TruncInst *trunc_inst) -> void;
  auto LowerSitoFpInst(middleend::llvm::inst::SitoFpInst *sitofp_inst) -> void;
  auto LowerFptoSiInst(middleend::llvm::inst::FptoSiInst *fptosi_inst) -> void;
  auto LowerUitoFpInst(middleend::llvm::inst::UitoFpInst *uitofp_inst) -> void;

  // Function-related instructions
  auto LowerCallInst(middleend::llvm::inst::CallInst *call_inst) -> void;
  auto LowerRetInst(middleend::llvm::inst::RetInst *return_inst) -> void;

  // Branch-related instructions
  auto LowerBrInst(middleend::llvm::inst::BrInst *br_inst) -> void;
  auto LowerBrCondInst(middleend::llvm::inst::BrCondInst *cond_br_inst) -> void;
  auto LowerPhiInst(middleend::llvm::inst::PhiInst *phi_inst) -> void;

  // Memory-related instructions
  auto LowerAllocaInst(middleend::llvm::inst::AllocaInst *alloca_inst) -> void;
  auto LowerLoadInst(middleend::llvm::inst::LoadInst *load_inst) -> void;
  auto LowerStoreInst(middleend::llvm::inst::StoreInst *store_inst) -> void;
  auto LowerGetElementPtrInst(middleend::llvm::inst::GetElementPtrInst *gep_inst) -> void;

  std::queue<backend::inst::PhiInst *> pending_phi_mirs_;
  std::queue<middleend::llvm::inst::PhiInst *> pending_phi_llvmirs_;
};

}  // namespace backend::lower

#endif
