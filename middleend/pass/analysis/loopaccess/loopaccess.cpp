#include "middleend/pass/analysis/loopaccess/loopaccess.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
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
  dependences_.clear();
  loops_analyzed_ = affine_accesses_ = rejected_non_gep_ = rejected_non_affine_ = 0;
  no_dependence_pairs_ = intra_iteration_pairs_ = loop_carried_pairs_ = unknown_pairs_ = 0;
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
      const auto &accesses = accesses_[loop];
      auto &dependences = dependences_[loop];
      for (size_t first = 0; first < accesses.size(); ++first) {
        for (size_t second = first + 1; second < accesses.size(); ++second) {
          if (accesses[first].kind == AccessKind::Load && accesses[second].kind == AccessKind::Load) continue;
          auto dependence = ClassifyDependence(accesses[first], accesses[second], loop);
          dependence.first = first;
          dependence.second = second;
          dependences.push_back(dependence);
          if (dependence.kind == DependenceKind::NoDependence) ++no_dependence_pairs_;
          else if (dependence.kind == DependenceKind::IntraIteration) ++intra_iteration_pairs_;
          else if (dependence.kind == DependenceKind::LoopCarried) ++loop_carried_pairs_;
          else ++unknown_pairs_;
        }
      }
    }
  }
  if (std::getenv("LOOP_ACCESS_STATS") != nullptr) {
    std::fprintf(stderr,
                 "[LoopAccess] loops=%zu accesses=%zu affine=%zu non-gep=%zu non-affine=%zu no-dep=%zu intra=%zu carried=%zu unknown=%zu\n",
                 loops_analyzed_, affine_accesses_ + rejected_non_gep_ + rejected_non_affine_, affine_accesses_,
                 rejected_non_gep_, rejected_non_affine_, no_dependence_pairs_, intra_iteration_pairs_,
                 loop_carried_pairs_, unknown_pairs_);
  }
  Validate();
}

auto LoopAccessAnalysisPass::ClassifyDependence(const Access &first, const Access &second, Loop *loop) -> Dependence {
  int distance = 0;
  if (first.affine && second.affine && ConstantDistance(first.pointer, second.pointer, loop, &distance)) {
    return {0, 0, distance == 0 ? DependenceKind::IntraIteration : DependenceKind::LoopCarried, distance};
  }
  // AliasAnalysis proves that two addresses in one dynamic iteration differ.  That is insufficient
  // for a loop-carried query when both are affine views of the same base, so it is consulted only
  // after the recurrence-distance proof above.
  if (alias_->Alias(first.pointer, second.pointer) == AliasResult::NoAlias) {
    return {0, 0, DependenceKind::NoDependence, 0};
  }
  return {0, 0, DependenceKind::Unknown, 0};
}

auto LoopAccessAnalysisPass::ConstantDistance(mlir::Value *first, mlir::Value *second, Loop *loop, int *distance) const
    -> bool {
  auto *lhs = first == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(first->op_);
  auto *rhs = second == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(second->op_);
  if (lhs == nullptr || rhs == nullptr || lhs->GetPtr() != rhs->GetPtr()) return false;
  auto lhs_indices = lhs->GetIndices();
  auto rhs_indices = rhs->GetIndices();
  if (lhs_indices.size() != rhs_indices.size()) return false;
  bool found = false;
  for (size_t index = 0; index < lhs_indices.size(); ++index) {
    const SCEV *lhs_scev = scev_->GetSCEV(lhs_indices[index]);
    const SCEV *rhs_scev = scev_->GetSCEV(rhs_indices[index]);
    if (lhs_scev == rhs_scev) continue;
    if (found || lhs_scev->GetKind() != SCEVKind::AddRec || rhs_scev->GetKind() != SCEVKind::AddRec) return false;
    auto *lhs_rec = static_cast<const SCEVAddRecExpr *>(lhs_scev);
    auto *rhs_rec = static_cast<const SCEVAddRecExpr *>(rhs_scev);
    if (!lhs_rec->IsAffine() || !rhs_rec->IsAffine() || !lhs_rec->IsNoWrap() || !rhs_rec->IsNoWrap() ||
        lhs_rec->GetLoop() != loop || rhs_rec->GetLoop() != loop || lhs_rec->GetStep() != rhs_rec->GetStep() ||
        lhs_rec->GetStep()->GetKind() != SCEVKind::Constant) {
      return false;
    }
    int step = static_cast<const SCEVConstant *>(lhs_rec->GetStep())->GetInt();
    int delta = 0;
    if (step == 0 || !ConstantDifference(lhs_rec->GetStart(), rhs_rec->GetStart(), &delta) || delta % step != 0) {
      return false;
    }
    *distance = delta / step;
    found = true;
  }
  if (!found) *distance = 0;
  return true;
}

auto LoopAccessAnalysisPass::ConstantDifference(const SCEV *first, const SCEV *second, int *difference) const -> bool {
  std::vector<const SCEV *> first_terms;
  std::vector<const SCEV *> second_terms;
  int64_t first_constant = 0;
  int64_t second_constant = 0;
  auto collect = [&](const SCEV *expression, auto &&collect_ref, std::vector<const SCEV *> *terms, int64_t *constant) -> void {
    if (expression->GetKind() == SCEVKind::Constant) {
      const auto *value = static_cast<const SCEVConstant *>(expression);
      if (!value->IsInt()) {
        terms->push_back(expression);
      } else {
        *constant += value->GetInt();
      }
      return;
    }
    if (expression->GetKind() == SCEVKind::Add) {
      for (const auto *operand : static_cast<const SCEVAddExpr *>(expression)->GetOperands()) {
        collect_ref(operand, collect_ref, terms, constant);
      }
      return;
    }
    terms->push_back(expression);
  };
  collect(first, collect, &first_terms, &first_constant);
  collect(second, collect, &second_terms, &second_constant);
  auto by_order = [](const SCEV *lhs, const SCEV *rhs) { return lhs->GetOrder() < rhs->GetOrder(); };
  std::sort(first_terms.begin(), first_terms.end(), by_order);
  std::sort(second_terms.begin(), second_terms.end(), by_order);
  if (first_terms != second_terms) return false;
  int64_t delta = second_constant - first_constant;
  if (delta < std::numeric_limits<int>::min() || delta > std::numeric_limits<int>::max()) return false;
  *difference = static_cast<int>(delta);
  return true;
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

auto LoopAccessAnalysisPass::GetDependences(Loop *loop) const -> const std::vector<Dependence> & {
  static const std::vector<Dependence> empty;
  auto iter = dependences_.find(loop);
  return iter == dependences_.end() ? empty : iter->second;
}

}  // namespace middleend::pass::analysis
