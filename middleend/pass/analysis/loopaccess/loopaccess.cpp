#include "middleend/pass/analysis/loopaccess/loopaccess.h"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/scev/scev.h"
#include "middleend/pass/analysis/scev/scevpass.h"

namespace middleend::pass::analysis {

auto LoopAccessAnalysisPass::Run() -> void {
  PreRun();
  accesses_.clear();
  loops_analyzed_ = affine_accesses_ = rejected_non_gep_ = rejected_non_affine_ = 0;
  auto *pass_context = compiler_context_->GetPassContext();
  auto *cfg = pass_context->GetSpecPass<CFGPass>("CFGPass");
  loop_info_ = pass_context->GetSpecPass<LoopInfoPass>("LoopInfoPass");
  scev_ = pass_context->GetSpecPass<SCEVPass>("SCEVPass");
  alias_ = pass_context->GetSpecPass<AliasAnalysisPass>("AliasAnalysisPass");
  if (cfg == nullptr || loop_info_ == nullptr || scev_ == nullptr || alias_ == nullptr) {
    throw std::logic_error("LoopAccessAnalysisPass requires CFG, LoopInfo, SCEV and AliasAnalysis");
  }
  for (auto *function_cfg : cfg->Get()) {
    for (auto *loop : loop_info_->GetLoops(function_cfg)) {
      ++loops_analyzed_;
      for (auto *block : loop->GetBlocks()) {
        for (auto *operation : block->operations_) {
          if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
            RecordAccess(load->GetPtr(), AccessKind::Load, loop);
          } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
            RecordAccess(store->GetPtr(), AccessKind::Store, loop);
          }
        }
      }
    }
  }
  if (std::getenv("LOOP_ACCESS_STATS") != nullptr) {
    std::fprintf(stderr, "[LoopAccess] loops=%zu accesses=%zu affine=%zu non-gep=%zu non-affine=%zu\n", loops_analyzed_,
                 affine_accesses_ + rejected_non_gep_ + rejected_non_affine_, affine_accesses_, rejected_non_gep_,
                 rejected_non_affine_);
  }
  Validate();
}

auto LoopAccessAnalysisPass::RecordAccess(mlir::Value *pointer, AccessKind kind, Loop *loop) -> void {
  auto *gep = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
  if (gep == nullptr) {
    ++rejected_non_gep_;
    return;
  }
  bool affine = true;
  bool variant = false;
  for (auto *index : gep->GetIndices()) {
    const SCEV *scev = scev_->GetSCEV(index);
    if (scev_->IsLoopInvariant(scev, loop)) continue;
    if (scev->GetKind() == SCEVKind::AddRec) {
      const auto *recurrence = static_cast<const SCEVAddRecExpr *>(scev);
      if (recurrence->GetLoop() == loop && recurrence->IsAffine()) {
        variant = true;
        continue;
      }
    }
    affine = false;
    break;
  }
  auto object = alias_->GetBaseObject(pointer);
  accesses_[loop].push_back({kind, pointer, object.root, affine, variant});
  if (affine) ++affine_accesses_;
  else ++rejected_non_affine_;
}

auto LoopAccessAnalysisPass::GetAccesses(Loop *loop) const -> const std::vector<Access> & {
  static const std::vector<Access> empty;
  auto iter = accesses_.find(loop);
  return iter == accesses_.end() ? empty : iter->second;
}

}  // namespace middleend::pass::analysis
