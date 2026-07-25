#include "middleend/pass/transform/memoization/memoization.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <utility>
#include "context/compiler_context.h"
#include "context/mlir_context.h"
#include "context/pass_context.h"
#include "context/type_context.h"
#include "middleend/mlir/dialect/base/base.h"
#include "middleend/mlir/dialect/func/return.h"
#include "middleend/mlir/dialect/var/var.h"
#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"
#include "utils/type/type.h"

#define GET_MLIR_CONTEXT() compiler_context_->GetIRContext()->GetMLIRContext()
#define GET_TYPE_CONTEXT() compiler_context_->GetTypeContext()
#define GET_PASS_CONTEXT() compiler_context_->GetPassContext()

namespace middleend::pass::transform {
namespace {
using middleend::mlir::Block;
using middleend::mlir::Operation;
using middleend::mlir::Value;

auto IsInt(type::Type *type) -> bool { return type != nullptr && type->GetKind() == type::TypeKind::INT; }

auto IsIntOrBool(type::Type *type) -> bool {
  return type != nullptr && (type->GetKind() == type::TypeKind::INT || type->GetKind() == type::TypeKind::BOOL);
}

auto IsIntBinaryOrCompare(Operation *op) -> bool {
  using namespace middleend::mlir::base;
  return mlir::DynamicOrNull<AddOp>(op) != nullptr || mlir::DynamicOrNull<SubOp>(op) != nullptr ||
         mlir::DynamicOrNull<MulOp>(op) != nullptr || mlir::DynamicOrNull<DivOp>(op) != nullptr ||
         mlir::DynamicOrNull<ModOp>(op) != nullptr || mlir::DynamicOrNull<EqualOp>(op) != nullptr ||
         mlir::DynamicOrNull<NotEqualOp>(op) != nullptr || mlir::DynamicOrNull<LessOp>(op) != nullptr ||
         mlir::DynamicOrNull<LessEqualOp>(op) != nullptr || mlir::DynamicOrNull<GreaterOp>(op) != nullptr ||
         mlir::DynamicOrNull<GreaterEqualOp>(op) != nullptr;
}

auto IsPureScalarOp(Operation *op) -> bool {
  using namespace middleend::mlir::base;
  auto scalar_typed = [&]() {
    for (auto *operand : op->operands_) {
      if (!IsIntOrBool(operand->GetType())) {
        return false;
      }
    }
    for (auto *result : op->results_) {
      if (!IsIntOrBool(result->GetType())) {
        return false;
      }
    }
    return true;
  };

  if (mlir::DynamicOrNull<ConstIntOp>(op) != nullptr) {
    return scalar_typed();
  }
  if (mlir::DynamicOrNull<CopyOp>(op) != nullptr || mlir::DynamicOrNull<PhiOp>(op) != nullptr ||
      mlir::DynamicOrNull<NotOp>(op) != nullptr) {
    return scalar_typed();
  }
  return IsIntBinaryOrCompare(op) && scalar_typed();
}
}  // namespace

auto MemoizationPass::Run() -> void {
  if (std::getenv("MEMO_OFF") != nullptr) {
    return;
  }

  PreRun();
  CollectModuleInfo();
  ComputeSCCs();
  ComputePureFunctions();

  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  std::vector<Candidate> candidates;
  for (auto *op : module->GetBlock()->operations_) {
    auto *func = mlir::DynamicOrNull<mlir::base::FuncDefOp>(op);
    if (func == nullptr || mlir::base::IsLibraryFunction(func->GetName())) {
      continue;
    }
    if (pure_functions_.count(func->GetName()) == 0) {
      continue;
    }
    auto candidate = Analyze(func);
    if (IsCandidate(candidate)) {
      candidates.emplace_back(std::move(candidate));
    }
  }

  if (candidates.empty()) {
    return;
  }

  auto names = ExistingGlobalNames();
  std::vector<CacheGlobals> all_globals;
  all_globals.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    all_globals.emplace_back(CreateCacheGlobals(candidate, &names));
  }
  InsertGlobalsBeforeFunctions(all_globals);

  for (size_t i = 0; i < candidates.size(); ++i) {
    RewriteFunction(candidates[i], all_globals[i]);
  }

  if (std::getenv("MEMO_STATS") != nullptr) {
    std::cerr << "[MemoizationPass] memoized " << candidates.size() << " function(s):";
    for (const auto &candidate : candidates) {
      std::cerr << " " << candidate.func->GetName();
    }
    std::cerr << "\n";
  }

  GET_PASS_CONTEXT()->InvalidateAnalysis();
}

void MemoizationPass::CollectModuleInfo() {
  functions_.clear();
  calls_.clear();
  scc_id_.clear();
  sccs_.clear();
  pure_functions_.clear();
  global_roots_.clear();
  readonly_global_roots_.clear();
  global_writes_.clear();
  direct_call_sites_.clear();
  cfg_by_name_.clear();
  dom_by_name_.clear();

  auto *cfg_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::CFGPass>("CFGPass");
  auto *dom_tree_pass = GET_PASS_CONTEXT()->GetSpecPass<analysis::DomTreePass>("DomTreePass");
  if (cfg_pass != nullptr && dom_tree_pass != nullptr) {
    for (auto *function_cfg : cfg_pass->Get()) {
      auto *mlir_cfg = dynamic_cast<analysis::MLIRFunctionCFG *>(function_cfg);
      if (mlir_cfg == nullptr || mlir_cfg->GetFuncOp() == nullptr) {
        continue;
      }
      const auto name = mlir_cfg->GetFuncOp()->GetName();
      cfg_by_name_[name] = mlir_cfg;
      dom_by_name_[name] = dom_tree_pass->GetDomTree(mlir_cfg);
    }
  }

  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  std::unordered_set<Value *> written_globals;
  for (auto *op : module->GetBlock()->operations_) {
    if (auto *global = mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(op)) {
      global_roots_[global->GetRes()] = global;
      if (global->IsConst()) {
        readonly_global_roots_.insert(global->GetRes());
      }
      continue;
    }
    auto *func = mlir::DynamicOrNull<mlir::base::FuncDefOp>(op);
    if (func == nullptr || mlir::base::IsLibraryFunction(func->GetName())) {
      continue;
    }
    functions_[func->GetName()] = func;
    calls_[func->GetName()];
    for (auto *block : func->GetBlocks()) {
      for (auto *inner : block->operations_) {
        if (auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(inner)) {
          calls_[func->GetName()].insert(call->GetName());
          direct_call_sites_[call->GetName()].push_back(MemoryEvent{func->GetName(), block, inner});
        } else if (auto *store = mlir::DynamicOrNull<mlir::var::VarSetOp>(inner)) {
          auto *root = GetAccessRoot(store->GetPtr());
          if (global_roots_.count(root) > 0) {
            written_globals.insert(root);
            global_writes_[root].push_back(MemoryEvent{func->GetName(), block, inner});
          }
        }
      }
    }
  }

  for (const auto &entry : global_roots_) {
    if (written_globals.count(entry.first) == 0) {
      readonly_global_roots_.insert(entry.first);
    }
  }
}

void MemoizationPass::ComputeSCCs() {
  std::unordered_map<std::string, int> index;
  std::unordered_map<std::string, int> lowlink;
  std::unordered_set<std::string> on_stack;
  std::vector<std::string> stack;
  int next_index = 0;

  std::function<void(const std::string &)> strong_connect = [&](const std::string &name) {
    index[name] = next_index;
    lowlink[name] = next_index;
    ++next_index;
    stack.emplace_back(name);
    on_stack.insert(name);

    for (const auto &callee : calls_[name]) {
      if (functions_.count(callee) == 0) {
        continue;
      }
      if (index.count(callee) == 0) {
        strong_connect(callee);
        lowlink[name] = std::min(lowlink[name], lowlink[callee]);
      } else if (on_stack.count(callee) > 0) {
        lowlink[name] = std::min(lowlink[name], index[callee]);
      }
    }

    if (lowlink[name] == index[name]) {
      std::vector<std::string> scc;
      while (true) {
        std::string member = stack.back();
        stack.pop_back();
        on_stack.erase(member);
        scc_id_[member] = static_cast<int>(sccs_.size());
        scc.emplace_back(member);
        if (member == name) {
          break;
        }
      }
      sccs_.emplace_back(std::move(scc));
    }
  };

  for (const auto &entry : functions_) {
    if (index.count(entry.first) == 0) {
      strong_connect(entry.first);
    }
  }
}

void MemoizationPass::ComputePureFunctions() {
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &scc : sccs_) {
      bool already_pure = true;
      for (const auto &name : scc) {
        if (pure_functions_.count(name) == 0) {
          already_pure = false;
          break;
        }
      }
      if (already_pure) {
        continue;
      }

      bool pure_scc = true;
      for (const auto &name : scc) {
        auto candidate = Analyze(functions_[name]);
        if (candidate.func == nullptr) {
          pure_scc = false;
          break;
        }
      }
      if (!pure_scc) {
        continue;
      }

      for (const auto &name : scc) {
        pure_functions_.insert(name);
      }
      changed = true;
    }
  }
}

auto MemoizationPass::Analyze(mlir::base::FuncDefOp *func) -> Candidate {
  Candidate candidate;
  candidate.func = func;
  candidate.args = func->GetArguments();
  candidate.recursive_scc = scc_id_.count(func->GetName()) > 0 && (sccs_[scc_id_[func->GetName()]].size() > 1 ||
                                                                   calls_[func->GetName()].count(func->GetName()) > 0);
  candidate.mutual_scc = scc_id_.count(func->GetName()) > 0 && sccs_[scc_id_[func->GetName()]].size() > 1;
  candidate.calltime_readonly_global_roots = GetCallTimeReadOnlyGlobalRoots(func->GetName());

  for (auto *block : func->GetBlocks()) {
    for (auto *op : block->operations_) {
      auto *def = mlir::DynamicOrNull<mlir::var::VarDefOp>(op);
      if (def == nullptr) {
        continue;
      }
      if (!def->GetDimensions().empty()) {
        continue;
      }
      bool int_inits = true;
      for (auto *init : def->GetInits()) {
        if (init != nullptr && !IsInt(init->GetType())) {
          int_inits = false;
          break;
        }
      }
      if (int_inits) {
        candidate.local_scalar_roots.insert(def->GetRes());
      }
    }
  }

  for (auto *block : func->GetBlocks()) {
    for (auto *op : block->operations_) {
      if (!IsPermittedPureOp(op, func->GetName(), candidate.local_scalar_roots, &candidate.self_calls,
                             candidate.calltime_readonly_global_roots, &candidate.recursive_scc_calls,
                             &candidate.returns)) {
        candidate.func = nullptr;
        return candidate;
      }
      auto *call = mlir::DynamicOrNull<mlir::base::FuncCallOp>(op);
      if (call != nullptr && call->GetName() == func->GetName()) {
        candidate.recursive_call_sites.emplace_back(block, op);
      }
    }
  }
  return candidate;
}

auto MemoizationPass::IsCandidate(const Candidate &candidate) -> bool {
  if (candidate.func == nullptr) {
    return false;
  }
  if (candidate.func->GetName() == "main") {
    return false;
  }
  if (!IsInt(candidate.func->GetReturnType())) {
    return false;
  }
  if (candidate.args.empty() || candidate.args.size() > 4) {
    return false;
  }
  for (auto *arg : candidate.args) {
    if (!IsInt(arg->GetType())) {
      return false;
    }
  }
  if (candidate.returns.empty()) {
    return false;
  }
  if (candidate.mutual_scc) {
    return candidate.recursive_scc_calls > 0;
  }
  if (candidate.self_calls < 2 || !HasMultipleCallsPerInvocation(candidate)) {
    return false;
  }
  return true;
}

auto MemoizationPass::HasMultipleCallsPerInvocation(const Candidate &candidate) -> bool {
  if (candidate.recursive_call_sites.size() < 2) {
    return false;
  }

  auto cfg_iter = cfg_by_name_.find(candidate.func->GetName());
  if (cfg_iter == cfg_by_name_.end() || cfg_iter->second == nullptr) {
    return false;
  }
  auto *cfg = cfg_iter->second;

  auto block_reaches = [&](Block *from, Block *target) {
    auto *start = cfg->GetNodeByBlock(from);
    auto *goal = cfg->GetNodeByBlock(target);
    if (start == nullptr || goal == nullptr) {
      return false;
    }
    std::vector<analysis::CFGNode *> worklist(start->GetSuccessors());
    std::unordered_set<analysis::CFGNode *> visited;
    while (!worklist.empty()) {
      auto *node = worklist.back();
      worklist.pop_back();
      if (node == goal) {
        return true;
      }
      if (node == nullptr || !visited.insert(node).second) {
        continue;
      }
      auto successors = node->GetSuccessors();
      worklist.insert(worklist.end(), successors.begin(), successors.end());
    }
    return false;
  };

  for (size_t lhs = 0; lhs < candidate.recursive_call_sites.size(); ++lhs) {
    for (size_t rhs = lhs + 1; rhs < candidate.recursive_call_sites.size(); ++rhs) {
      auto *lhs_block = candidate.recursive_call_sites[lhs].first;
      auto *rhs_block = candidate.recursive_call_sites[rhs].first;
      if (lhs_block == rhs_block || block_reaches(lhs_block, rhs_block) || block_reaches(rhs_block, lhs_block)) {
        return true;
      }
    }
  }
  return false;
}

auto MemoizationPass::IsPermittedPureOp(Operation *op, const std::string &func_name,
                                        const std::unordered_set<Value *> &local_scalar_roots, int *self_calls,
                                        const std::unordered_set<Value *> &calltime_readonly_global_roots,
                                        int *recursive_scc_calls, std::vector<Operation *> *returns) -> bool {
  using namespace middleend::mlir;

  if (mlir::DynamicOrNull<base::BrOp>(op) != nullptr || mlir::DynamicOrNull<base::CondBrOp>(op) != nullptr) {
    return true;
  }

  if (auto *ret = mlir::DynamicOrNull<func::ReturnOp>(op)) {
    if (ret->GetReturnValue() == nullptr || !IsInt(ret->GetReturnValue()->GetType())) {
      return false;
    }
    returns->emplace_back(op);
    return true;
  }

  if (auto *call = mlir::DynamicOrNull<base::FuncCallOp>(op)) {
    for (auto *arg : call->GetOperands()) {
      if (!IsInt(arg->GetType())) {
        return false;
      }
    }
    if (!IsInt(call->GetRes()->GetType())) {
      return false;
    }
    if (call->GetName() == func_name) {
      ++(*self_calls);
      ++(*recursive_scc_calls);
      return true;
    }
    if (IsSameRecursiveSCC(func_name, call->GetName())) {
      ++(*recursive_scc_calls);
      return true;
    }
    if (pure_functions_.count(call->GetName()) > 0) {
      return true;
    }
    return false;
  }

  if (mlir::DynamicOrNull<var::GlobalVarDefOp>(op) != nullptr) {
    return false;
  }
  if (mlir::DynamicOrNull<var::VarDefOp>(op) != nullptr || mlir::DynamicOrNull<var::VarGetPtrOp>(op) != nullptr ||
      mlir::DynamicOrNull<var::VarGetValOp>(op) != nullptr || mlir::DynamicOrNull<var::VarSetOp>(op) != nullptr) {
    return IsLocalScalarMemoryOp(op, local_scalar_roots) ||
           IsReadOnlyGlobalMemoryOp(op, calltime_readonly_global_roots);
  }

  return IsPureScalarOp(op);
}

auto MemoizationPass::IsSameRecursiveSCC(const std::string &lhs, const std::string &rhs) -> bool {
  if (scc_id_.count(lhs) == 0 || scc_id_.count(rhs) == 0 || scc_id_[lhs] != scc_id_[rhs]) {
    return false;
  }
  int id = scc_id_[lhs];
  return sccs_[id].size() > 1 || calls_[lhs].count(lhs) > 0;
}

auto MemoizationPass::GetCallTimeReadOnlyGlobalRoots(const std::string &func_name) -> std::unordered_set<Value *> {
  std::unordered_set<Value *> result;
  if (scc_id_.count(func_name) == 0) {
    return result;
  }

  const int target_scc = scc_id_[func_name];
  const auto &members = sccs_[target_scc];
  std::unordered_set<std::string> member_set(members.begin(), members.end());

  std::vector<MemoryEvent> external_calls;
  for (const auto &member : members) {
    for (const auto &call : direct_call_sites_[member]) {
      if (member_set.count(call.func_name) == 0) {
        external_calls.emplace_back(call);
      }
    }
  }
  if (external_calls.empty()) {
    return result;
  }

  for (const auto &global_entry : global_roots_) {
    auto *root = global_entry.first;
    if (readonly_global_roots_.count(root) > 0) {
      continue;
    }
    const auto write_it = global_writes_.find(root);
    if (write_it == global_writes_.end() || write_it->second.empty()) {
      continue;
    }

    bool written_inside_scc = false;
    for (const auto &write : write_it->second) {
      if (member_set.count(write.func_name) > 0) {
        written_inside_scc = true;
        break;
      }
    }
    if (written_inside_scc) {
      continue;
    }

    bool all_writes_dominate_all_calls = true;
    for (const auto &write : write_it->second) {
      for (const auto &call : external_calls) {
        if (!EventDominates(write, call)) {
          all_writes_dominate_all_calls = false;
          break;
        }
      }
      if (!all_writes_dominate_all_calls) {
        break;
      }
    }
    if (all_writes_dominate_all_calls) {
      result.insert(root);
    }
  }

  return result;
}

auto MemoizationPass::OperationIndex(Block *block, Operation *op) -> int {
  if (block == nullptr || op == nullptr) {
    return -1;
  }
  for (size_t i = 0; i < block->operations_.size(); ++i) {
    if (block->operations_[i] == op) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

auto MemoizationPass::EventDominates(const MemoryEvent &write, const MemoryEvent &call) -> bool {
  if (write.func_name != call.func_name) {
    return false;
  }
  if (write.block == nullptr || call.block == nullptr || write.op == nullptr || call.op == nullptr) {
    return false;
  }
  if (write.block == call.block) {
    const int write_index = OperationIndex(write.block, write.op);
    const int call_index = OperationIndex(call.block, call.op);
    return write_index >= 0 && call_index >= 0 && write_index < call_index;
  }

  auto cfg_it = cfg_by_name_.find(write.func_name);
  auto dom_it = dom_by_name_.find(write.func_name);
  if (cfg_it == cfg_by_name_.end() || dom_it == dom_by_name_.end() || dom_it->second == nullptr) {
    return false;
  }
  auto *write_node = cfg_it->second->GetNodeByBlock(write.block);
  auto *call_node = cfg_it->second->GetNodeByBlock(call.block);
  return write_node != nullptr && call_node != nullptr && dom_it->second->Dominates(write_node, call_node);
}

auto MemoizationPass::GetAccessRoot(Value *ptr) -> Value * {
  auto *root = ptr;
  while (root != nullptr) {
    auto *get_ptr = mlir::DynamicOrNull<mlir::var::VarGetPtrOp>(root->op_);
    if (get_ptr == nullptr) {
      break;
    }
    root = get_ptr->GetPtr();
  }
  return root;
}

auto MemoizationPass::IsLocalScalarMemoryOp(Operation *op,
                                            const std::unordered_set<Value *> &local_scalar_roots) -> bool {
  using namespace middleend::mlir;

  if (auto *def = mlir::DynamicOrNull<var::VarDefOp>(op)) {
    if (local_scalar_roots.count(def->GetRes()) == 0 || !def->GetDimensions().empty()) {
      return false;
    }
    for (auto *init : def->GetInits()) {
      if (init != nullptr && !IsInt(init->GetType())) {
        return false;
      }
    }
    return true;
  }

  if (auto *get_ptr = mlir::DynamicOrNull<var::VarGetPtrOp>(op)) {
    if (!get_ptr->GetIndices().empty()) {
      return false;
    }
    return local_scalar_roots.count(GetAccessRoot(get_ptr->GetPtr())) > 0;
  }

  if (auto *load = mlir::DynamicOrNull<var::VarGetValOp>(op)) {
    return IsInt(load->GetRes()->GetType()) && local_scalar_roots.count(GetAccessRoot(load->GetPtr())) > 0;
  }

  if (auto *store = mlir::DynamicOrNull<var::VarSetOp>(op)) {
    return IsInt(store->GetNewValue()->GetType()) && local_scalar_roots.count(GetAccessRoot(store->GetPtr())) > 0;
  }

  return false;
}

auto MemoizationPass::IsReadOnlyGlobalMemoryOp(
    Operation *op, const std::unordered_set<Value *> &calltime_readonly_global_roots) -> bool {
  using namespace middleend::mlir;

  if (auto *get_ptr = mlir::DynamicOrNull<var::VarGetPtrOp>(op)) {
    auto *root = GetAccessRoot(get_ptr->GetPtr());
    if (readonly_global_roots_.count(root) == 0 && calltime_readonly_global_roots.count(root) == 0) {
      return false;
    }
    for (auto *index : get_ptr->GetIndices()) {
      if (!IsInt(index->GetType())) {
        return false;
      }
    }
    return true;
  }

  if (auto *load = mlir::DynamicOrNull<var::VarGetValOp>(op)) {
    auto *root = GetAccessRoot(load->GetPtr());
    return IsInt(load->GetRes()->GetType()) &&
           (readonly_global_roots_.count(root) > 0 || calltime_readonly_global_roots.count(root) > 0);
  }

  return false;
}

auto MemoizationPass::ExistingGlobalNames() -> std::unordered_set<std::string> {
  std::unordered_set<std::string> names;
  auto *module = GET_MLIR_CONTEXT()->GetModuleOp();
  for (auto *op : module->GetBlock()->operations_) {
    if (auto *global = mlir::DynamicOrNull<mlir::var::GlobalVarDefOp>(op)) {
      names.insert(global->GetName());
    }
  }
  return names;
}

auto MemoizationPass::MakeUniqueName(const std::string &base, std::unordered_set<std::string> *names) -> std::string {
  std::string name = base;
  int suffix = 0;
  while (names->count(name) > 0) {
    name = base + "_" + std::to_string(++suffix);
  }
  names->insert(name);
  return name;
}

auto MemoizationPass::CreateCacheGlobals(const Candidate &candidate,
                                         std::unordered_set<std::string> *names) -> CacheGlobals {
  CacheGlobals globals;
  auto *int_type = GET_TYPE_CONTEXT()->GetIntegerType();
  auto *array_type = GET_TYPE_CONTEXT()->GetArrayType(int_type, kCacheSize);
  const std::vector<int> dims{kCacheSize};
  const std::string prefix = "__memo_" + candidate.func->GetName();

  auto make_global = [&](const std::string &suffix) -> mlir::var::GlobalVarDefOp * {
    auto *global = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::GlobalVarDefOp>(
        GET_MLIR_CONTEXT(), MakeUniqueName(prefix + suffix, names), dims, std::vector<int>{}, false);
    global->GetRes()->SetType(array_type);
    return global;
  };

  globals.valid = make_global("_valid");
  globals.value = make_global("_value");
  globals.keys.reserve(candidate.args.size());
  for (size_t i = 0; i < candidate.args.size(); ++i) {
    globals.keys.emplace_back(make_global("_key" + std::to_string(i)));
  }
  return globals;
}

auto MemoizationPass::MakeBlock(const std::string &func_name, const std::string &suffix) -> Block * {
  auto *block = new Block();
  block->block_name_ = func_name + "_memo_" + suffix;
  block->block_id_ = static_cast<int>(next_block_id_++);
  return block;
}

auto MemoizationPass::MakeValue(type::Type *type, const std::string &name) -> Value * {
  auto *value = GET_MLIR_CONTEXT()->CreateValue();
  value->SetType(type);
  value->name_ = name;
  return value;
}

void MemoizationPass::RegisterUses(Operation *op) {
  for (size_t i = 0; i < op->operands_.size(); ++i) {
    op->operands_[i]->AddUse(op, i);
  }
}

void MemoizationPass::AddOperation(Block *block, Operation *op) {
  RegisterUses(op);
  block->AddOperation(op);
}

void MemoizationPass::InsertBefore(Block *block, Operation *before, Operation *op) {
  RegisterUses(op);
  auto it = std::find(block->operations_.begin(), block->operations_.end(), before);
  if (it == block->operations_.end()) {
    throw std::logic_error("MemoizationPass: return op not found in containing block");
  }
  block->operations_.insert(it, op);
}

auto MemoizationPass::MakeConstInt(int value, Block *block) -> Value * {
  auto *result = MakeValue(GET_TYPE_CONTEXT()->GetIntegerType());
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(result, value);
  AddOperation(block, op);
  return result;
}

template <typename BinaryOp>
auto MemoizationPass::MakeBinary(Value *lhs, Value *rhs, type::Type *result_type, Block *block,
                                 const std::string &name) -> Value * {
  auto *result = MakeValue(result_type, name);
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<BinaryOp>(result, lhs, rhs);
  AddOperation(block, op);
  return result;
}

auto MemoizationPass::MakeGetPtr(Value *base, Value *index, Block *block) -> Value * {
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(GET_MLIR_CONTEXT(), base,
                                                                         std::vector<Value *>{index});
  op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(GET_TYPE_CONTEXT()->GetIntegerType()));
  AddOperation(block, op);
  return op->GetRes();
}

auto MemoizationPass::MakeLoad(Value *ptr, type::Type *result_type, Block *block, const std::string &name) -> Value * {
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetValOp>(GET_MLIR_CONTEXT(), ptr);
  op->GetRes()->SetType(result_type);
  op->GetRes()->name_ = name;
  AddOperation(block, op);
  return op->GetRes();
}

auto MemoizationPass::MakeStore(Value *ptr, Value *value, Block *block) -> void {
  auto *op = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), ptr, value);
  op->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
  AddOperation(block, op);
}

auto MemoizationPass::RewriteFunction(const Candidate &candidate, const CacheGlobals &globals) -> void {
  auto *func = candidate.func;
  auto old_blocks = func->GetBlocks();
  auto *old_entry = old_blocks.front();
  auto *int_type = GET_TYPE_CONTEXT()->GetIntegerType();
  auto *bool_type = GET_TYPE_CONTEXT()->GetBooleanType();

  auto *entry = MakeBlock(func->GetName(), "entry");
  auto *neg = MakeBlock(func->GetName(), "neg");
  auto *probe = MakeBlock(func->GetName(), "probe");
  std::vector<Block *> key_blocks;
  key_blocks.reserve(candidate.args.size());
  for (size_t i = 0; i < candidate.args.size(); ++i) {
    key_blocks.emplace_back(MakeBlock(func->GetName(), "key" + std::to_string(i)));
  }
  auto *hit = MakeBlock(func->GetName(), "hit");

  auto *zero = MakeConstInt(0, entry);
  auto *cache_size = MakeConstInt(kCacheSize, entry);
  auto *hash_mul = MakeConstInt(kHashMul, entry);
  Value *hash = zero;
  for (auto *arg : candidate.args) {
    auto *mixed = MakeBinary<mlir::base::MulOp>(hash, hash_mul, int_type, entry);
    hash = MakeBinary<mlir::base::AddOp>(mixed, arg, int_type, entry);
  }
  auto *raw_slot = MakeBinary<mlir::base::ModOp>(hash, cache_size, int_type, entry, "memo_raw");
  auto *is_neg = MakeBinary<mlir::base::LessOp>(raw_slot, zero, bool_type, entry, "memo_is_neg");
  AddOperation(entry, GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(is_neg, neg, probe));

  auto *adjusted_slot = MakeBinary<mlir::base::AddOp>(raw_slot, cache_size, int_type, neg, "memo_slot_adj");
  AddOperation(neg, GET_MLIR_CONTEXT()->CreateOperation<mlir::base::BrOp>(probe));

  auto *slot = MakeValue(int_type, "memo_slot");
  auto *slot_phi = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::PhiOp>(
      slot, std::vector<Value *>{raw_slot, adjusted_slot}, std::vector<Block *>{entry, neg});
  AddOperation(probe, slot_phi);
  auto *valid_ptr = MakeGetPtr(globals.valid->GetRes(), slot, probe);
  auto *valid = MakeLoad(valid_ptr, int_type, probe, "memo_valid");
  auto *has_value = MakeBinary<mlir::base::NotEqualOp>(valid, zero, bool_type, probe, "memo_has_value");
  AddOperation(probe,
               GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(has_value, key_blocks.front(), old_entry));

  for (size_t i = 0; i < key_blocks.size(); ++i) {
    auto *block = key_blocks[i];
    auto *key_ptr = MakeGetPtr(globals.keys[i]->GetRes(), slot, block);
    auto *key = MakeLoad(key_ptr, int_type, block, "memo_key");
    auto *matches = MakeBinary<mlir::base::EqualOp>(key, candidate.args[i], bool_type, block, "memo_key_match");
    auto *true_target = (i + 1 == key_blocks.size()) ? hit : key_blocks[i + 1];
    AddOperation(block, GET_MLIR_CONTEXT()->CreateOperation<mlir::base::CondBrOp>(matches, true_target, old_entry));
  }

  auto *value_ptr = MakeGetPtr(globals.value->GetRes(), slot, hit);
  auto *cached = MakeLoad(value_ptr, int_type, hit, "memo_cached");
  auto *return_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::func::ReturnOp>(GET_MLIR_CONTEXT(), int_type, cached);
  AddOperation(hit, return_op);

  RewriteState state;
  state.probe_block = probe;
  state.slot_value = slot;
  state.globals = globals;
  for (auto *ret : candidate.returns) {
    AddCacheStores(ret, candidate, state);
  }

  std::vector<Block *> new_blocks{entry, neg, probe};
  new_blocks.insert(new_blocks.end(), key_blocks.begin(), key_blocks.end());
  new_blocks.emplace_back(hit);
  new_blocks.insert(new_blocks.end(), old_blocks.begin(), old_blocks.end());
  func->SetBlocks(std::move(new_blocks));
}

void MemoizationPass::AddCacheStores(Operation *return_op, const Candidate &candidate, const RewriteState &state) {
  auto *ret = mlir::DynamicOrNull<mlir::func::ReturnOp>(return_op);
  if (ret == nullptr || ret->GetReturnValue() == nullptr) {
    return;
  }

  Block *block = nullptr;
  for (auto *candidate_block : candidate.func->GetBlocks()) {
    for (auto *op : candidate_block->operations_) {
      if (op == return_op) {
        block = candidate_block;
        break;
      }
    }
    if (block != nullptr) {
      break;
    }
  }
  if (block == nullptr) {
    throw std::logic_error("MemoizationPass: cannot locate return block");
  }

  std::vector<Operation *> stores;
  stores.reserve(candidate.args.size() + 2);
  for (size_t i = 0; i < candidate.args.size(); ++i) {
    auto *ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
        GET_MLIR_CONTEXT(), state.globals.keys[i]->GetRes(), std::vector<Value *>{state.slot_value});
    ptr->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(GET_TYPE_CONTEXT()->GetIntegerType()));
    stores.emplace_back(ptr);
    auto *store =
        GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), ptr->GetRes(), candidate.args[i]);
    store->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
    stores.emplace_back(store);
  }
  auto *value_ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
      GET_MLIR_CONTEXT(), state.globals.value->GetRes(), std::vector<Value *>{state.slot_value});
  value_ptr->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(GET_TYPE_CONTEXT()->GetIntegerType()));
  stores.emplace_back(value_ptr);
  auto *value_store = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), value_ptr->GetRes(),
                                                                               ret->GetReturnValue());
  value_store->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
  stores.emplace_back(value_store);
  auto *valid_ptr = GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarGetPtrOp>(
      GET_MLIR_CONTEXT(), state.globals.valid->GetRes(), std::vector<Value *>{state.slot_value});
  valid_ptr->GetRes()->SetType(GET_TYPE_CONTEXT()->GetPtrType(GET_TYPE_CONTEXT()->GetIntegerType()));
  stores.emplace_back(valid_ptr);
  auto *one = GET_MLIR_CONTEXT()->CreateValue();
  one->SetType(GET_TYPE_CONTEXT()->GetIntegerType());
  auto *one_op = GET_MLIR_CONTEXT()->CreateOperation<mlir::base::ConstIntOp>(one, 1);
  stores.emplace_back(one_op);
  auto *valid_store =
      GET_MLIR_CONTEXT()->CreateOperation<mlir::var::VarSetOp>(GET_MLIR_CONTEXT(), valid_ptr->GetRes(), one);
  valid_store->GetRes()->SetType(GET_TYPE_CONTEXT()->GetUnknownType());
  stores.emplace_back(valid_store);

  for (auto *op : stores) {
    InsertBefore(block, return_op, op);
  }
}

void MemoizationPass::InsertGlobalsBeforeFunctions(const std::vector<CacheGlobals> &globals) {
  auto *module_block = GET_MLIR_CONTEXT()->GetModuleOp()->GetBlock();
  std::vector<Operation *> flat_globals;
  for (const auto &group : globals) {
    flat_globals.emplace_back(group.valid);
    flat_globals.emplace_back(group.value);
    for (auto *key : group.keys) {
      flat_globals.emplace_back(key);
    }
  }

  auto insert_pos = module_block->operations_.begin();
  while (insert_pos != module_block->operations_.end() &&
         mlir::DynamicOrNull<mlir::base::FuncDefOp>(*insert_pos) == nullptr) {
    ++insert_pos;
  }
  module_block->operations_.insert(insert_pos, flat_globals.begin(), flat_globals.end());
}

}  // namespace middleend::pass::transform
