#include "middleend/pass/analysis/sideeffect/sideeffect.h"

#include <cstdio>
#include <deque>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/funccall.h"
#include "middleend/mlir/dialect/base/funcdef.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/value/value.h"
#include "middleend/pass/analysis/cfg/cfg.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/pass.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::analysis {
namespace {

// --- helpers ---------------------------------------------------------------

bool IsLibrary(const std::string &name) {
  static const std::unordered_set<std::string> kLib = {"getint", "getch", "getfloat", "getarray", "getfarray",
                                                       "putint", "putch", "putfloat", "putarray", "putfarray",
                                                       "_sysy_starttime", "_sysy_stoptime"};
  return kLib.count(name) > 0;
}

bool IsLibInput(const std::string &name) {
  static const std::unordered_set<std::string> kIn = {"getint", "getch", "getfloat", "getarray", "getfarray"};
  return kIn.count(name) > 0;
}

bool IsLibOutput(const std::string &name) {
  static const std::unordered_set<std::string> kOut = {"putint", "putch", "putfloat", "putarray", "putfarray",
                                                       "_sysy_starttime", "_sysy_stoptime"};
  return kOut.count(name) > 0;
}

bool IsLibArrayInput(const std::string &name) { return name == "getarray" || name == "getfarray"; }
bool IsLibArrayOutput(const std::string &name) { return name == "putarray" || name == "putfarray"; }

// Collect FuncDefOp entries keyed by name. The module block holds both
// base::FuncDefOp (lib declarations) and func::FuncDefOp (user definitions).
auto CollectFunctions(mlir::ModuleOp *module) -> std::unordered_map<std::string, mlir::Operation *> {
  std::unordered_map<std::string, mlir::Operation *> funcs;
  for (auto *op : module->GetBlock()->operations_) {
    if (auto *f = mlir::DynamicOrNull<mlir::func::FuncDefOp>(op)) {
      funcs[f->GetName()] = op;
    } else if (auto *b = mlir::DynamicOrNull<mlir::base::FuncDefOp>(op)) {
      funcs.try_emplace(b->GetName(), op);
    }
  }
  return funcs;
}

auto GetCallName(mlir::Operation *op) -> std::string {
  if (auto *c = mlir::DynamicOrNull<mlir::base::FuncCallOp>(op)) return c->GetName();
  if (auto *c = mlir::DynamicOrNull<mlir::func::FuncCallOp>(op)) return c->GetName();
  return {};
}

bool IsCallOp(mlir::Operation *op) {
  return mlir::DynamicOrNull<mlir::base::FuncCallOp>(op) != nullptr ||
         mlir::DynamicOrNull<mlir::func::FuncCallOp>(op) != nullptr;
}

// Walk the getptr chain to the root pointer.
auto RootPointer(mlir::Value *ptr) -> mlir::Value * {
  while (ptr != nullptr && ptr->op_ != nullptr) {
    auto *gpo = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(ptr->op_);
    if (gpo == nullptr) break;
    ptr = gpo->GetPtr();
  }
  return ptr;
}

}  // namespace

// ---- public ---------------------------------------------------------------

auto FunctionSideEffectPass::Run() -> void {
  PreRun();
  if (std::getenv("SE_OFF") != nullptr) return;

  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<CFGPass>("CFGPass");
  if (cfg_pass == nullptr) throw std::logic_error("FunctionSideEffectPass requires CFGPass");

  summaries_.clear();
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  auto funcs = CollectFunctions(module);

  // Library functions: fixed table-driven summary.
  for (const auto &[name, _] : funcs)
    if (IsLibrary(name)) ComputeLibrary(name);

  // Bottom-up fixed-point propagation driven by the CFG so we iterate
  // blocks the same way every other pass does.
  bool changed = true;
  int iterations = 0;
  while (changed && iterations++ < 64) {
    changed = false;
    for (auto *func_cfg : cfg_pass->Get()) {
      auto *mlir_cfg = dynamic_cast<MLIRFunctionCFG *>(func_cfg);
      if (mlir_cfg == nullptr) continue;
      auto *fop = mlir_cfg->GetFuncOp();
      if (fop == nullptr) continue;
      std::string name;
      if (auto *f = mlir::DynamicOrNull<mlir::func::FuncDefOp>(fop)) name = f->GetName();
      else if (auto *b = mlir::DynamicOrNull<mlir::base::FuncDefOp>(fop)) name = b->GetName();
      if (summaries_.count(name) > 0) continue;
      ComputeOne(mlir_cfg);
      if (summaries_.count(name) > 0) changed = true;
    }
    // SCC pass for mutual recursion (rare in SysY).
    for (const auto &[name, _] : funcs) {
      if (summaries_.count(name) == 0) {
        std::unordered_set<std::string> scc{name};
        ComputeSCC(&scc, cfg_pass);
        if (summaries_.count(name) > 0) changed = true;
        break;
      }
    }
  }
}

auto FunctionSideEffectPass::GetSummary(const std::string &func_name) const -> const SideEffectSummary & {
  static const SideEffectSummary kConservative{true, true, {}, {}, false, false, false, {}};
  auto it = summaries_.find(func_name);
  return it != summaries_.end() ? it->second : kConservative;
}

auto FunctionSideEffectPass::IsReadNoneAndIOClean(mlir::Operation *call) -> bool {
  std::string callee;
  if (auto *bc = mlir::DynamicOrNull<mlir::base::FuncCallOp>(call)) callee = bc->GetName();
  else if (auto *fc = mlir::DynamicOrNull<mlir::func::FuncCallOp>(call)) callee = fc->GetName();
  if (callee.empty() || !HasSummary(callee)) return false;
  const auto &s = GetSummary(callee);
  // A call with no IO effect and no memory writes has no observable side effect.
  return !s.reads_io && !s.writes_io && s.IsWriteNone();
}

bool FunctionSideEffectPass::HasSummary(const std::string &func_name) const {
  return summaries_.count(func_name) > 0;
}

// ---- library table --------------------------------------------------------

void FunctionSideEffectPass::ComputeLibrary(const std::string &name) {
  SideEffectSummary s;
  s.reads_io = IsLibInput(name);
  s.writes_io = IsLibOutput(name);
  if (IsLibArrayInput(name)) {
    s.arg_effects.resize(1);
    s.arg_effects[0] = SideEffectSummary::ArgEffect::kWrite;
  }
  if (IsLibArrayOutput(name)) {
    s.arg_effects.resize(1);
    s.arg_effects[0] = SideEffectSummary::ArgEffect::kRead;
  }
  s.saves_pointer = false;
  summaries_[name] = s;
}

// ---- single-function analysis ---------------------------------------------

void FunctionSideEffectPass::ComputeOne(MLIRFunctionCFG *mlir_cfg) {
  auto *fop = mlir_cfg->GetFuncOp();
  if (fop == nullptr) return;
  std::string name;
  if (auto *f = mlir::DynamicOrNull<mlir::func::FuncDefOp>(fop)) name = f->GetName();
  else if (auto *b = mlir::DynamicOrNull<mlir::base::FuncDefOp>(fop)) name = b->GetName();
  if (name.empty()) return;

  SideEffectSummary s;
  std::unordered_set<mlir::Value *> pointer_params;
  for (auto *param : fop->operands_) {
    if (param->GetType() != nullptr && param->GetType()->GetKind() == type::TypeKind::PTR)
      pointer_params.insert(param);
  }

  bool has_unresolved_callee = false;

  for (auto *cfg_node : mlir_cfg->GetNodes()) {
    auto *mlir_node = dynamic_cast<MLIRCFGNode *>(cfg_node);
    if (mlir_node == nullptr) continue;
    for (auto *op : mlir_node->GetBlock()->operations_) {
      // --- store ----------------------------------------------------------
      if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(op)) {
        auto *root = RootPointer(store->GetPtr());
        if (root != nullptr && root->op_ != nullptr &&
            mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(root->op_) != nullptr)
          s.written_globals.insert(root);
        // A store whose base is not rooted in any IR operation (var.def /
        // var.globaldef) targets memory the caller owns — typically a
        // pointer parameter. Mark conservatively.
        if (root != nullptr && root->op_ == nullptr) s.writes_all_globals = true;
        if (pointer_params.count(store->GetNewValue()) > 0) s.saves_pointer = true;
        continue;
      }

      // --- call -----------------------------------------------------------
      if (!IsCallOp(op)) continue;
      std::string callee = GetCallName(op);
      if (callee.empty()) continue;

      if (IsLibrary(callee)) {
        const auto &ls = GetSummary(callee);
        if (ls.reads_io) s.reads_io = true;
        if (ls.writes_io) s.writes_io = true;
        if (ls.saves_pointer) s.saves_pointer = true;
        continue;
      }

      auto it = summaries_.find(callee);
      if (it != summaries_.end()) {
        const auto &ss = it->second;
        if (ss.reads_all_globals) s.reads_all_globals = true;
        if (ss.writes_all_globals) s.writes_all_globals = true;
        for (auto *g : ss.read_globals) s.read_globals.insert(g);
        for (auto *g : ss.written_globals) s.written_globals.insert(g);
        if (ss.reads_io) s.reads_io = true;
        if (ss.writes_io) s.writes_io = true;
        if (ss.saves_pointer) s.saves_pointer = true;
      } else {
        has_unresolved_callee = true;
      }
    }
  }

  if (has_unresolved_callee) return;
  summaries_[name] = s;
}

// ---- SCC handling ---------------------------------------------------------

void FunctionSideEffectPass::ComputeSCC(std::unordered_set<std::string> *scc_nodes, CFGPass *cfg_pass) {
  for (const auto &name : *scc_nodes)
    summaries_[name] = SideEffectSummary{true, true, {}, {}, false, false, false, {}};

  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &name : *scc_nodes) {
      SideEffectSummary old = summaries_[name];
      summaries_.erase(name);
      // Find the CFG for this SCC member.
      for (auto *func_cfg : cfg_pass->Get()) {
        auto *mlir_cfg = dynamic_cast<MLIRFunctionCFG *>(func_cfg);
        if (mlir_cfg == nullptr) continue;
        auto *fop = mlir_cfg->GetFuncOp();
        if (fop == nullptr) continue;
        std::string fname;
        if (auto *f = mlir::DynamicOrNull<mlir::func::FuncDefOp>(fop)) fname = f->GetName();
        else if (auto *b = mlir::DynamicOrNull<mlir::base::FuncDefOp>(fop)) fname = b->GetName();
        if (fname != name) continue;
        ComputeOne(mlir_cfg);
        break;
      }
      if (summaries_.count(name) == 0) {
        summaries_[name] = old;
        continue;
      }
      const auto &s = summaries_[name];
      if (old.reads_all_globals && !s.reads_all_globals) changed = true;
      if (old.writes_all_globals && !s.writes_all_globals) changed = true;
    }
  }
}

}  // namespace middleend::pass::analysis
