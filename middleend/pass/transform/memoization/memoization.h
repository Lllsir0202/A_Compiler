#ifndef MIDDLEEND_PASS_TRANSFORM_MEMOIZATION_MEMOIZATION_H
#define MIDDLEEND_PASS_TRANSFORM_MEMOIZATION_MEMOIZATION_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "middleend/pass/transform/transform.h"

namespace type {
class Type;
}  // namespace type

namespace middleend::mlir {
class Block;
class Operation;
class Value;
}  // namespace middleend::mlir

namespace middleend::mlir::base {
class FuncDefOp;
}  // namespace middleend::mlir::base

namespace middleend::mlir::var {
class GlobalVarDefOp;
}  // namespace middleend::mlir::var

namespace middleend::pass::transform {

/**
 * @brief Memoize pure recursive integer functions with a conservative, collision-safe
 * direct-mapped cache.
 *
 * This is intentionally placed in BASEIR, after the normal scalar/loop pipeline.  At
 * this level the function body has already been lowered to explicit calls and memory
 * operations, so the safety rule can stay explicit and auditable: integer functions
 * may use scalar frame-local temporaries, call functions proven pure in the same
 * recursive SCC / helper DAG, and read globals that are const or never written by the
 * program.  Mutable global writes, IO, escaping pointers, array parameters, and
 * impure calls still reject the transform.  A cache collision is handled by comparing
 * every key field before returning a cached value; a mismatch falls through to the
 * original body and overwrites that slot on return.
 */
class MemoizationPass : public TransformPass {
 public:
  MemoizationPass(context::CompilerContext *compiler_context, PassStage stage)
      : TransformPass(compiler_context, stage) {
    if (stage != PassStage::BASEIR) {
      throw std::invalid_argument("MemoizationPass only supports BASEIR stage");
    }
  }
  ~MemoizationPass() override = default;

  auto Run() -> void override;
  auto Name() const -> std::string override { return "MemoizationPass"; }

 private:
  struct Candidate {
    mlir::base::FuncDefOp *func{nullptr};
    std::vector<mlir::Value *> args;
    std::vector<mlir::Operation *> returns;
    std::unordered_set<mlir::Value *> local_scalar_roots;
    int self_calls{0};
    int recursive_scc_calls{0};
    bool recursive_scc{false};
    bool mutual_scc{false};
  };

  struct CacheGlobals {
    mlir::var::GlobalVarDefOp *valid{nullptr};
    mlir::var::GlobalVarDefOp *value{nullptr};
    std::vector<mlir::var::GlobalVarDefOp *> keys;
  };

  struct RewriteState {
    mlir::Block *probe_block{nullptr};
    mlir::Value *slot_value{nullptr};
    CacheGlobals globals;
  };

  void CollectModuleInfo();
  void ComputeSCCs();
  void ComputePureFunctions();
  auto Analyze(mlir::base::FuncDefOp *func) -> Candidate;
  auto IsCandidate(const Candidate &candidate) -> bool;
  auto IsPermittedPureOp(mlir::Operation *op, const std::string &func_name,
                         const std::unordered_set<mlir::Value *> &local_scalar_roots, int *self_calls,
                         int *recursive_scc_calls, std::vector<mlir::Operation *> *returns) -> bool;
  auto IsLocalScalarMemoryOp(mlir::Operation *op, const std::unordered_set<mlir::Value *> &local_scalar_roots) -> bool;
  auto IsReadOnlyGlobalMemoryOp(mlir::Operation *op) -> bool;
  auto GetAccessRoot(mlir::Value *ptr) -> mlir::Value *;
  auto IsSameRecursiveSCC(const std::string &lhs, const std::string &rhs) -> bool;
  auto ExistingGlobalNames() -> std::unordered_set<std::string>;
  auto MakeUniqueName(const std::string &base, std::unordered_set<std::string> *names) -> std::string;
  auto CreateCacheGlobals(const Candidate &candidate, std::unordered_set<std::string> *names) -> CacheGlobals;
  auto RewriteFunction(const Candidate &candidate, const CacheGlobals &globals) -> void;
  auto MakeBlock(const std::string &func_name, const std::string &suffix) -> mlir::Block *;
  auto MakeValue(type::Type *type, const std::string &name = "") -> mlir::Value *;
  auto MakeConstInt(int value, mlir::Block *block) -> mlir::Value *;
  template <typename BinaryOp>
  auto MakeBinary(mlir::Value *lhs, mlir::Value *rhs, type::Type *result_type, mlir::Block *block,
                  const std::string &name = "") -> mlir::Value *;
  auto MakeGetPtr(mlir::Value *base, mlir::Value *index, mlir::Block *block) -> mlir::Value *;
  auto MakeLoad(mlir::Value *ptr, type::Type *result_type, mlir::Block *block,
                const std::string &name = "") -> mlir::Value *;
  auto MakeStore(mlir::Value *ptr, mlir::Value *value, mlir::Block *block) -> void;
  void AddOperation(mlir::Block *block, mlir::Operation *op);
  void InsertBefore(mlir::Block *block, mlir::Operation *before, mlir::Operation *op);
  void RegisterUses(mlir::Operation *op);
  void AddCacheStores(mlir::Operation *return_op, const Candidate &candidate, const RewriteState &state);
  void InsertGlobalsBeforeFunctions(const std::vector<CacheGlobals> &globals);

  std::size_t next_block_id_{970000};
  std::unordered_map<std::string, mlir::base::FuncDefOp *> functions_;
  std::unordered_map<std::string, std::unordered_set<std::string>> calls_;
  std::unordered_map<std::string, int> scc_id_;
  std::vector<std::vector<std::string>> sccs_;
  std::unordered_set<std::string> pure_functions_;
  std::unordered_map<mlir::Value *, mlir::var::GlobalVarDefOp *> global_roots_;
  std::unordered_set<mlir::Value *> readonly_global_roots_;
  static constexpr int kCacheSize = 32768;
  static constexpr int kHashMul = 1315423911;
};

}  // namespace middleend::pass::transform

#endif
