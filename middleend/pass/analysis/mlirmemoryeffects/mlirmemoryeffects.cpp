#include "middleend/pass/analysis/mlirmemoryeffects/mlirmemoryeffects.h"
#include <algorithm>
#include <unordered_set>
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/copy.h"
#include "middleend/mlir/dialect/arithm/unaryop.h"
#include "middleend/mlir/dialect/func/funccall.h"
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/dialect/scf/while.h"
#include "middleend/mlir/dialect/var/def.h"
#include "middleend/mlir/dialect/var/getptr.h"
#include "middleend/mlir/dialect/var/getvalue.h"
#include "middleend/mlir/dialect/var/globaldef.h"
#include "middleend/mlir/dialect/var/set.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
#include "middleend/mlir/utils/value/value.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()

namespace middleend::pass::analysis {
namespace {

auto IsLibrary(const std::string &name) -> bool {
  static const std::unordered_set<std::string> libraries = {
      "getint", "getch",    "getfloat", "getarray",  "getfarray",       "putint",
      "putch",  "putfloat", "putarray", "putfarray", "_sysy_starttime", "_sysy_stoptime",
  };
  return libraries.count(name) > 0;
}

auto LibrarySummary(const std::string &name) -> SideEffectSummary {
  SideEffectSummary summary;
  summary.reads_io =
      name == "getint" || name == "getch" || name == "getfloat" || name == "getarray" || name == "getfarray";
  summary.writes_io = name == "putint" || name == "putch" || name == "putfloat" || name == "putarray" ||
                      name == "putfarray" || name == "_sysy_starttime" || name == "_sysy_stoptime";
  if (name == "getarray" || name == "getfarray") {
    summary.arg_effects.resize(1, SideEffectSummary::ArgEffect::kWrite);
  } else if (name == "putarray" || name == "putfarray") {
    summary.arg_effects.resize(2, SideEffectSummary::ArgEffect::kNone);
    summary.arg_effects[1] = SideEffectSummary::ArgEffect::kRead;
  }
  return summary;
}

auto MergeEffect(SideEffectSummary::ArgEffect lhs, SideEffectSummary::ArgEffect rhs) -> SideEffectSummary::ArgEffect {
  auto reads = [](SideEffectSummary::ArgEffect effect) {
    return effect == SideEffectSummary::ArgEffect::kRead || effect == SideEffectSummary::ArgEffect::kReadWrite;
  };
  auto writes = [](SideEffectSummary::ArgEffect effect) {
    return effect == SideEffectSummary::ArgEffect::kWrite || effect == SideEffectSummary::ArgEffect::kReadWrite;
  };
  bool may_read = reads(lhs) || reads(rhs);
  bool may_write = writes(lhs) || writes(rhs);
  if (may_read && may_write) return SideEffectSummary::ArgEffect::kReadWrite;
  if (may_read) return SideEffectSummary::ArgEffect::kRead;
  if (may_write) return SideEffectSummary::ArgEffect::kWrite;
  return SideEffectSummary::ArgEffect::kNone;
}

auto EffectReads(SideEffectSummary::ArgEffect effect) -> bool {
  return effect == SideEffectSummary::ArgEffect::kRead || effect == SideEffectSummary::ArgEffect::kReadWrite;
}

auto EffectWrites(SideEffectSummary::ArgEffect effect) -> bool {
  return effect == SideEffectSummary::ArgEffect::kWrite || effect == SideEffectSummary::ArgEffect::kReadWrite;
}

auto SummariesEqual(const SideEffectSummary &lhs, const SideEffectSummary &rhs) -> bool {
  return lhs.reads_all_globals == rhs.reads_all_globals && lhs.writes_all_globals == rhs.writes_all_globals &&
         lhs.read_globals == rhs.read_globals && lhs.written_globals == rhs.written_globals &&
         lhs.reads_io == rhs.reads_io && lhs.writes_io == rhs.writes_io && lhs.saves_pointer == rhs.saves_pointer &&
         lhs.arg_effects == rhs.arg_effects;
}

auto TraceCopies(mlir::Value *value) -> mlir::Value * {
  std::unordered_set<mlir::Value *> visited;
  while (value != nullptr && visited.insert(value).second) {
    auto *copy = mlir::DynamicOrNull<mlir::arithm::CopyOp>(value->op_);
    if (copy == nullptr) break;
    value = copy->GetValue();
  }
  return value;
}

}  // namespace

auto MLIRMemoryEffectAnalysis::Run() -> void {
  functions_.clear();
  summaries_.clear();
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  if (module == nullptr) {
    throw std::logic_error("Module operation is not set in MLIRContext");
  }

  for (auto *operation : module->GetBlock()->operations_) {
    auto *function = mlir::DynamicOrNull<mlir::func::FuncDefOp>(operation);
    if (function == nullptr) continue;
    functions_[function->GetName()] = function;
    summaries_[function->GetName()] =
        IsLibrary(function->GetName()) ? LibrarySummary(function->GetName()) : SideEffectSummary{};
  }

  bool changed = true;
  for (size_t iteration = 0; changed && iteration < functions_.size() + 1; ++iteration) {
    changed = false;
    for (const auto &[name, function] : functions_) {
      if (IsLibrary(name)) continue;
      auto summary = AnalyzeFunction(function);
      if (!SummariesEqual(summary, summaries_[name])) {
        summaries_[name] = std::move(summary);
        changed = true;
      }
    }
  }
  Validate();
}

auto MLIRMemoryEffectAnalysis::GetSummary(const std::string &name) const -> const SideEffectSummary & {
  static const SideEffectSummary conservative{true, true, {}, {}, true, true, true, {}};
  auto iter = summaries_.find(name);
  return iter == summaries_.end() ? conservative : iter->second;
}

auto MLIRMemoryEffectAnalysis::GetPointerRoot(mlir::Value *pointer) -> PointerRoot {
  pointer = TraceCopies(pointer);
  std::unordered_set<mlir::Value *> visited;
  while (pointer != nullptr && visited.insert(pointer).second) {
    if (auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_)) {
      pointer = TraceCopies(get_ptr->GetPtr());
      continue;
    }
    if (mlir::DynamicOrNull<mlir::var::VarDefOp>(pointer->op_) != nullptr) {
      return {RootKind::kLocal, pointer};
    }
    if (mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(pointer->op_) != nullptr) {
      return {RootKind::kGlobal, pointer};
    }
    if (pointer->op_ == nullptr) {
      return {RootKind::kParameter, pointer};
    }
    return {RootKind::kUnknown, pointer};
  }
  return {};
}

auto MLIRMemoryEffectAnalysis::RootsAreDistinct(const PointerRoot &lhs, const PointerRoot &rhs) -> bool {
  if (lhs.value == nullptr || rhs.value == nullptr || lhs.value == rhs.value) return false;
  if (lhs.kind == RootKind::kLocal || rhs.kind == RootKind::kLocal) return true;
  return lhs.kind == RootKind::kGlobal && rhs.kind == RootKind::kGlobal;
}

auto MLIRMemoryEffectAnalysis::MayAlias(mlir::Value *lhs, mlir::Value *rhs) -> bool {
  auto lhs_root = GetPointerRoot(lhs);
  auto rhs_root = GetPointerRoot(rhs);
  return !RootsAreDistinct(lhs_root, rhs_root);
}

auto MLIRMemoryEffectAnalysis::IsSafeToSpeculateLoad(mlir::Value *pointer) const -> bool {
  std::vector<mlir::Value *> indices;
  pointer = TraceCopies(pointer);
  while (pointer != nullptr) {
    auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(pointer->op_);
    if (get_ptr == nullptr) break;
    auto current = get_ptr->GetIndices();
    indices.insert(indices.begin(), current.begin(), current.end());
    pointer = TraceCopies(get_ptr->GetPtr());
  }

  const std::vector<int> *dimensions = nullptr;
  if (auto *local = pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::VarDefOp>(pointer->op_)) {
    dimensions = &local->GetDimensions();
  } else if (auto *global =
                 pointer == nullptr ? nullptr : mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(pointer->op_)) {
    dimensions = &global->GetDimensions();
  } else {
    return false;
  }
  if (indices.size() != dimensions->size()) return false;
  for (size_t index = 0; index < indices.size(); ++index) {
    auto *index_value = TraceCopies(indices[index]);
    auto *constant = index_value == nullptr ? nullptr : mlir::DynamicOrNull<mlir::arithm::ConstOp>(index_value->op_);
    if (constant == nullptr || !std::holds_alternative<int>(constant->GetValue())) return false;
    int value = std::get<int>(constant->GetValue());
    if (value < 0 || value >= (*dimensions)[index]) return false;
  }
  return true;
}

auto MLIRMemoryEffectAnalysis::AnalyzeFunction(mlir::func::FuncDefOp *function) const -> SideEffectSummary {
  SideEffectSummary summary;
  auto parameters = function->GetArguments();
  summary.arg_effects.resize(parameters.size(), SideEffectSummary::ArgEffect::kNone);
  for (auto *operation : function->GetEntryBlock()->operations_) {
    AnalyzeOperation(operation, parameters, &summary);
  }
  return summary;
}

auto MLIRMemoryEffectAnalysis::AnalyzeOperation(mlir::Operation *operation,
                                                const std::vector<mlir::Value *> &parameters,
                                                SideEffectSummary *summary) const -> void {
  auto record_access = [&](mlir::Value *pointer, bool writes) {
    auto root = GetPointerRoot(pointer);
    if (root.kind == RootKind::kGlobal) {
      (writes ? summary->written_globals : summary->read_globals).insert(root.value);
      return;
    }
    if (root.kind == RootKind::kParameter) {
      auto iter = std::find(parameters.begin(), parameters.end(), root.value);
      if (iter == parameters.end()) {
        writes ? summary->writes_all_globals = true : summary->reads_all_globals = true;
        return;
      }
      size_t index = static_cast<size_t>(std::distance(parameters.begin(), iter));
      auto effect = writes ? SideEffectSummary::ArgEffect::kWrite : SideEffectSummary::ArgEffect::kRead;
      summary->arg_effects[index] = MergeEffect(summary->arg_effects[index], effect);
      return;
    }
    if (root.kind == RootKind::kUnknown) {
      writes ? summary->writes_all_globals = true : summary->reads_all_globals = true;
    }
  };

  if (auto *load = mlir::DynamicOrNull<mlir::var::VarGetValOp>(operation)) {
    record_access(load->GetPtr(), false);
  } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    record_access(store->GetPtr(), true);
  } else if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    const auto &callee = GetSummary(call->GetName());
    summary->reads_all_globals = summary->reads_all_globals || callee.reads_all_globals;
    summary->writes_all_globals = summary->writes_all_globals || callee.writes_all_globals;
    summary->read_globals.insert(callee.read_globals.begin(), callee.read_globals.end());
    summary->written_globals.insert(callee.written_globals.begin(), callee.written_globals.end());
    summary->reads_io = summary->reads_io || callee.reads_io;
    summary->writes_io = summary->writes_io || callee.writes_io;
    summary->saves_pointer = summary->saves_pointer || callee.saves_pointer;
    auto arguments = call->GetOperands();
    for (size_t index = 0; index < callee.arg_effects.size() && index < arguments.size(); ++index) {
      if (EffectReads(callee.arg_effects[index])) record_access(arguments[index], false);
      if (EffectWrites(callee.arg_effects[index])) record_access(arguments[index], true);
    }
  }

  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        AnalyzeOperation(nested, parameters, summary);
      }
    }
  }
}

auto MLIRMemoryEffectAnalysis::IsReadNoneAndIOClean(mlir::func::FuncCallOp *call) const -> bool {
  if (call == nullptr || summaries_.count(call->GetName()) == 0) return false;
  const auto &summary = GetSummary(call->GetName());
  bool pointer_effects =
      std::any_of(summary.arg_effects.begin(), summary.arg_effects.end(),
                  [](SideEffectSummary::ArgEffect effect) { return effect != SideEffectSummary::ArgEffect::kNone; });
  return summary.IsMemoryReadNone() && summary.IsMemoryWriteNone() && !pointer_effects && !summary.reads_io &&
         !summary.writes_io;
}

auto MLIRMemoryEffectAnalysis::CallMayWrite(mlir::func::FuncCallOp *call, mlir::Value *pointer) const -> bool {
  if (call == nullptr || summaries_.count(call->GetName()) == 0) return true;
  const auto &summary = GetSummary(call->GetName());
  if (summary.writes_all_globals) return true;
  for (auto *global : summary.written_globals) {
    if (MayAlias(global, pointer)) return true;
  }
  auto arguments = call->GetOperands();
  for (size_t index = 0; index < summary.arg_effects.size() && index < arguments.size(); ++index) {
    if (EffectWrites(summary.arg_effects[index]) && MayAlias(arguments[index], pointer)) return true;
  }
  return false;
}

auto MLIRMemoryEffectAnalysis::OperationMayWrite(mlir::Operation *operation, mlir::Value *pointer) const -> bool {
  if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(operation)) {
    if (MayAlias(store->GetPtr(), pointer)) return true;
  } else if (auto *call = mlir::DynamicOrNull<mlir::func::FuncCallOp>(operation)) {
    if (CallMayWrite(call, pointer)) return true;
  }
  for (auto *region : operation->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *nested : block->operations_) {
        if (OperationMayWrite(nested, pointer)) return true;
      }
    }
  }
  return false;
}

auto MLIRMemoryEffectAnalysis::LoopMayWrite(mlir::scf::WhileOp *loop, mlir::Value *pointer) const -> bool {
  if (loop == nullptr || pointer == nullptr) return true;
  for (auto *region : loop->regions_) {
    for (auto *block : region->blocks_) {
      for (auto *operation : block->operations_) {
        if (OperationMayWrite(operation, pointer)) return true;
      }
    }
  }
  return false;
}

}  // namespace middleend::pass::analysis
