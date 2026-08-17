#include "middleend/pass/transform/mlirtiling/mlirtiling.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/arithm/binaryop.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/scf/break.h"
#include "middleend/mlir/dialect/scf/condition.h"
#include "middleend/mlir/dialect/scf/continue.h"
#include "middleend/mlir/dialect/scf/if.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/scf/yield.h"
#include "middleend/mlir/utils/clone/uselistrebuilder.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/transform/mlirloopunroll/inductionvariable.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::transform {
namespace {

constexpr int64_t kDefaultTileSize = 32;

auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  while (value != nullptr) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) {
      break;
    }
    value = copy->GetValue();
  }
  return value;
}

// True if `value` (after copies) is an int constant; fills `out`.
auto IsConstInt(mlir::Value *value, int64_t *out) -> bool {
  value = TraceCopies(value);
  auto *constant = value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(value->op_);
  if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) {
    return false;
  }
  *out = std::get<int>(constant->GetValue());
  return true;
}

// True if the loop's own body (descending into `scf.if` regions, but not into nested
// `scf.while`, whose escapes are their own concern) contains a `break`/`return`.
auto BodyEscapesLoop(mlir::Block *body) -> bool {
  for (auto *operation : body->operations_) {
    if (mlir::DynamicOrNull<mlir::scf::BreakOp>(operation) != nullptr ||
        mlir::DynamicOrNull<mlir::func::ReturnOp>(operation) != nullptr) {
      return true;
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
        if (region != nullptr && !region->blocks_.empty() && BodyEscapesLoop(region->blocks_[0])) {
          return true;
        }
      }
    }
  }
  return false;
}

// True if the loop's own body (descending into `scf.if` regions) contains a nested
// `scf.while` — i.e. the loop is not the innermost of its nest.
auto BodyHasNestedLoop(mlir::Block *body) -> bool {
  for (auto *operation : body->operations_) {
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(operation) != nullptr) {
      return true;
    }
    if (auto *if_op = mlir::DynamicOrNull<mlir::scf::IfOp>(operation)) {
      for (auto *region : {if_op->GetThenRegion(), if_op->GetElseRegion()}) {
        if (region != nullptr && !region->blocks_.empty() && BodyHasNestedLoop(region->blocks_[0])) {
          return true;
        }
      }
    }
  }
  return false;
}

auto EnvFlagEnabled(const char *name) -> bool {
  const char *value = std::getenv(name);
  return value != nullptr && std::string(value) != "0";
}

}  // namespace

auto MLIRLoopTilingPass::Run() -> void {
  PreRun();
  if (!EnvFlagEnabled("TILING_ON") || EnvFlagEnabled("TILING_OFF")) {
    return;
  }
  candidates_ = 0;
  tiled_ = 0;
  shape_rejects_ = 0;
  domain_rejects_ = 0;
  escape_rejects_ = 0;
  innermost_rejects_ = 0;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }
  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function != nullptr && !mlir::func::IsLibraryFunction(function->GetName())) {
      TileInFunction(function);
    }
  }
  if (std::getenv("LOOP_OPT_STATS") != nullptr) {
    std::cerr << "LoopTiling: candidates=" << candidates_ << " tiled=" << tiled_ << " shape=" << shape_rejects_
              << " domain=" << domain_rejects_ << " escape=" << escape_rejects_
              << " innermost=" << innermost_rejects_ << '\n';
  }
}

auto MLIRLoopTilingPass::TileInFunction(mlir::func::FuncDefOp *func_op) -> bool {
  mlir::utils::RebuildFunctionUses(func_op);
  bool changed = TileInBlock(func_op->GetEntryBlock());
  if (changed) {
    mlir::utils::RebuildFunctionUses(func_op);
  }
  return changed;
}

auto MLIRLoopTilingPass::TileInBlock(mlir::Block *block) -> bool {
  bool changed = false;
  // Bottom-up: tile inner loops before their enclosing loop, so a nest becomes
  // `kk -> k -> ii -> i -> jj -> j` with each tile loop wrapping its inner loop.
  std::vector<mlir::Operation *> original = block->operations_;
  for (auto *operation : original) {
    for (auto *region : operation->regions_) {
      for (auto *nested : region->blocks_) {
        changed = TileInBlock(nested) || changed;
      }
    }
  }
  // Tile the block's own loops, each at most once. Iterating a copy (and replacing
  // in place) means a freshly created tile loop at `index` is never re-tiled here.
  for (size_t index = 0; index < original.size(); ++index) {
    if (mlir::DynamicOrNull<mlir::scf::WhileOp>(original[index]) != nullptr) {
      if (TryTile(block, index)) {
        changed = true;
      }
    }
  }
  return changed;
}

auto MLIRLoopTilingPass::TryTile(mlir::Block *parent, size_t index) -> bool {
  auto *loop = mlir::DynamicOrNull<mlir::scf::WhileOp>(parent->operations_[index]);
  if (loop == nullptr) {
    return false;
  }
  auto iv = loopiv::Recognize(loop);
  // Canonical counted loop: `iv < bound`, `iv += 1`. The tile decomposition covers
  // `[ii, ii+T)` in induction-value units, which is only correct for unit steps
  // (e.g. a SHA-1 outer loop advancing chunk_start by 64 would be mis-tiled).
  if (!iv.valid_ || iv.kind_ != loopiv::CmpKind::kLT || iv.step_kind_ != loopiv::StepKind::kAdd || iv.step_ != 1 ||
      iv.init_ == nullptr || iv.bound_ == nullptr) {
    ++shape_rejects_;
    return false;
  }
  if (loopiv::IsDefinedInLoop(iv.init_, loop) || loopiv::IsDefinedInLoop(iv.bound_, loop)) {
    ++domain_rejects_;
    return false;
  }
  if (loop->GetCondRegion()->blocks_.size() != 1 || loop->GetBodyRegion()->blocks_.size() != 1) {
    ++shape_rejects_;
    return false;
  }
  auto *cond_block = loop->GetCondRegion()->blocks_[0];
  auto *body_block = loop->GetBodyRegion()->blocks_[0];
  auto *condition_op = cond_block->operations_.empty()
                           ? nullptr
                           : mlir::DynamicOrNull<mlir::scf::ConditionOp>(cond_block->operations_.back());
  auto *yield_op = body_block->operations_.empty()
                       ? nullptr
                       : mlir::DynamicOrNull<mlir::scf::YieldOp>(body_block->operations_.back());
  const size_t m = cond_block->arguments_.size();
  if (condition_op == nullptr || yield_op == nullptr || m != body_block->arguments_.size() ||
      iv.index_ >= m || condition_op->GetOperands().size() != m || yield_op->GetOperands().size() != m) {
    ++shape_rejects_;
    return false;
  }
  // `break`/`return` escape the loop and would be mis-targeted by cloning into the
  // inner loop; `continue` is safe (it targets the immediately-enclosing loop).
  if (BodyEscapesLoop(body_block)) {
    ++escape_rejects_;
    return false;
  }
  // Only tile non-innermost loops: the innermost loop is stride-1 contiguous and is
  // unrolling's job; strip-mining it would only add loop overhead.
  if (!BodyHasNestedLoop(body_block)) {
    ++innermost_rejects_;
    return false;
  }

  int64_t tile = kDefaultTileSize;
  if (const char *env = std::getenv("TILING_SIZE")) {
    int64_t parsed = std::atoll(env);
    if (parsed > 0) {
      tile = parsed;
    }
  }
  int64_t constant_trip = loopiv::ConstantTripCount(iv, INT32_MAX);
  if (constant_trip >= 0 && constant_trip < tile) {
    ++domain_rejects_;  // provably too small to benefit; leave scalar.
    return false;
  }
  // When the bound is a compile-time constant divisible by the tile, `min(ii+T, bound)`
  // is always `ii+T` (ii never exceeds bound - T), so no select is needed.
  int64_t bound_const = 0;
  bool bound_divisible = IsConstInt(iv.bound_, &bound_const) && bound_const % tile == 0;

  ++candidates_;
  const size_t i0 = iv.index_;
  auto *int_type = cond_block->arguments_[i0]->GetType();
  auto *bool_type = condition_op->GetCondition()->GetType();
  auto *const_tile_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::ConstOp>(
      GET_MLIR_CONTEXT(), static_cast<int>(tile));
  const_tile_op->GetRes()->SetType(int_type);

  // ---- Tile loop carried arguments: [ii, v_0..v_{m-1}] ----
  auto *tile_ii = GET_MLIR_CONTEXT()->CreateValue();
  tile_ii->SetType(int_type);
  std::vector<mlir::Value *> tile_args{tile_ii};
  std::vector<mlir::Value *> tile_v;  // tile_v[j] = tile-loop slot for original carried value j
  tile_v.reserve(m);
  for (size_t j = 0; j < m; ++j) {
    auto *value = GET_MLIR_CONTEXT()->CreateValue();
    value->SetType(cond_block->arguments_[j]->GetType());
    tile_args.emplace_back(value);
    tile_v.emplace_back(value);
  }
  auto *tile_cond_block = new mlir::Block();
  auto *tile_body_block = new mlir::Block();
  for (auto *arg : tile_args) {
    tile_cond_block->AddArgument(arg);
    tile_body_block->AddArgument(arg);
  }
  auto *tile_less = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), tile_ii, iv.bound_);
  tile_less->GetRes()->SetType(bool_type);
  tile_cond_block->AddOperation(tile_less);
  auto *tile_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), tile_less->GetRes(), std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> tile_inits;
  tile_inits.reserve(m + 1);
  tile_inits.emplace_back(iv.init_);  // ii starts at the original induction init
  for (size_t j = 0; j < m; ++j) {
    tile_inits.emplace_back(condition_op->GetOriginalValues()[j]);
  }
  tile_condition->SetLoopCarriedValues(tile_args, tile_inits);
  tile_cond_block->AddOperation(tile_condition);

  // ---- Tile loop body ----
  tile_body_block->AddOperation(const_tile_op);
  // limit = min(ii + T, bound)
  auto *iip_tile = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), tile_ii,
                                                                            const_tile_op->GetRes());
  iip_tile->GetRes()->SetType(int_type);
  tile_body_block->AddOperation(iip_tile);
  mlir::Value *limit = nullptr;
  if (bound_divisible) {
    limit = iip_tile->GetRes();
  } else {
    auto *limit_less =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), iip_tile->GetRes(), iv.bound_);
    limit_less->GetRes()->SetType(bool_type);
    tile_body_block->AddOperation(limit_less);
    auto make_yield_region = [&](mlir::Value *value) {
      auto *region = new mlir::Region();
      auto *region_block = new mlir::Block();
      region_block->operations_.emplace_back(
          GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), value));
      region->AddBlock(region_block);
      return region;
    };
    auto *min_if = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::IfOp>(
        GET_MLIR_CONTEXT(), 1, limit_less->GetRes(), make_yield_region(iip_tile->GetRes()),
        make_yield_region(iv.bound_));
    min_if->GetResultByIndex(0)->SetType(int_type);
    tile_body_block->AddOperation(min_if);
    limit = min_if->GetResultByIndex(0);
  }

  // ---- Inner loop: carries exactly L's carried values ----
  std::vector<mlir::Value *> inner_args;
  inner_args.reserve(m);
  for (size_t j = 0; j < m; ++j) {
    auto *value = GET_MLIR_CONTEXT()->CreateValue();
    value->SetType(cond_block->arguments_[j]->GetType());
    inner_args.emplace_back(value);
  }
  auto *inner_cond_block = new mlir::Block();
  auto *inner_body_block = new mlir::Block();
  for (auto *arg : inner_args) {
    inner_cond_block->AddArgument(arg);
    inner_body_block->AddArgument(arg);
  }
  auto *inner_less =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::LessOp>(GET_MLIR_CONTEXT(), inner_args[i0], limit);
  inner_less->GetRes()->SetType(bool_type);
  inner_cond_block->AddOperation(inner_less);
  auto *inner_condition = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::ConditionOp>(
      GET_MLIR_CONTEXT(), inner_less->GetRes(), std::unordered_map<symboltable::VarEntry *, mlir::Value *>{});
  std::vector<mlir::Value *> inner_inits;
  inner_inits.reserve(m);
  for (size_t j = 0; j < m; ++j) {
    inner_inits.emplace_back(j == i0 ? tile_ii : tile_v[j]);
  }
  inner_condition->SetLoopCarriedValues(inner_args, inner_inits);
  inner_cond_block->AddOperation(inner_condition);
  // Clone the original body into the inner loop, remapping the carried args.
  cloner_.value_map_.clear();
  for (size_t j = 0; j < m; ++j) {
    cloner_.value_map_[cond_block->arguments_[j]] = inner_args[j];
    cloner_.value_map_[body_block->arguments_[j]] = inner_args[j];
  }
  for (size_t op_index = 0; op_index + 1 < body_block->operations_.size(); ++op_index) {
    inner_body_block->AddOperation(cloner_.Clone(body_block->operations_[op_index]));
  }
  std::vector<mlir::Value *> inner_yields;
  inner_yields.reserve(m);
  for (size_t j = 0; j < m; ++j) {
    inner_yields.emplace_back(cloner_.RemapValue(yield_op->GetOperands()[j]));
  }
  inner_body_block->AddOperation(
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), inner_yields));
  auto *inner_cond_region = new mlir::Region();
  inner_cond_region->AddBlock(inner_cond_block);
  auto *inner_body_region = new mlir::Region();
  inner_body_region->AddBlock(inner_body_block);
  auto *inner_loop =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), m, inner_cond_region, inner_body_region);
  for (size_t j = 0; j < m; ++j) {
    inner_loop->GetResultByIndex(j)->SetType(loop->GetResultByIndex(j)->GetType());
  }
  tile_body_block->AddOperation(inner_loop);

  // ii' = ii + T; tile yield threads the inner loop's carried results.
  auto *ii_next = GET_MLIR_CONTEXT()->CreateOperation<mlir::arithm::AddOp>(GET_MLIR_CONTEXT(), tile_ii,
                                                                           const_tile_op->GetRes());
  ii_next->GetRes()->SetType(int_type);
  tile_body_block->AddOperation(ii_next);
  std::vector<mlir::Value *> tile_yields;
  tile_yields.reserve(m + 1);
  tile_yields.emplace_back(ii_next->GetRes());
  for (size_t j = 0; j < m; ++j) {
    tile_yields.emplace_back(inner_loop->GetResultByIndex(j));
  }
  tile_body_block->AddOperation(GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::YieldOp>(GET_MLIR_CONTEXT(), tile_yields));

  // ---- Assemble the tile loop ----
  auto *tile_cond_region = new mlir::Region();
  tile_cond_region->AddBlock(tile_cond_block);
  auto *tile_body_region = new mlir::Region();
  tile_body_region->AddBlock(tile_body_block);
  auto *tile_loop = GET_MLIR_CONTEXT()->CreateOperation<mlir::scf::WhileOp>(GET_MLIR_CONTEXT(), m + 1, tile_cond_region,
                                                                            tile_body_region);
  tile_loop->GetResultByIndex(0)->SetType(int_type);
  for (size_t j = 0; j < m; ++j) {
    tile_loop->GetResultByIndex(j + 1)->SetType(loop->GetResultByIndex(j)->GetType());
  }

  // Rewire uses of L's results to the tile loop's, and splice it in place of L.
  for (size_t j = 0; j < m; ++j) {
    loop->GetResultByIndex(j)->ReplaceAllUsesWith(tile_loop->GetResultByIndex(j + 1));
  }
  parent->operations_[index] = tile_loop;
  ++tiled_;
  return true;
}

}  // namespace middleend::pass::transform
