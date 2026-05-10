#ifndef MIDDLEEND_MLIR_DIALECT_BASE_FUNCDEF_H
#define MIDDLEEND_MLIR_DIALECT_BASE_FUNCDEF_H
#include <string>
#include <unordered_set>
#include <utility>
#include "context/mlir_context.h"
#include "middleend/mlir/dialect/base/basic.h"
#include "mlir/utils/struct/region.h"
#include "mlir/utils/value/value.h"
#include "utils/type/type.h"

namespace middleend::mlir::base {
#include "middleend/checklibrary.ck"

class FuncDefOp : public BaseOps {
 public:
  static auto Create(mlir::Value *res, const std::string &name, std::vector<mlir::Value *> args) -> FuncDefOp * {
    auto func_def_op = new FuncDefOp();
    func_def_op->SetUp(res, name, std::move(args));
    return func_def_op;
  }

  auto Emit(std::ofstream *out) -> void override {
    PrintIndent(out);
    results_[0]->Emit(out);
    // Print return type
    *out << ": " << results_[0]->GetType()->ToString() << " ";
    *out << " = " << Name() << "@" << name_ << "(";
    for (size_t i = 0; i < operands_.size(); ++i) {
      operands_[i]->Emit(out);
      *out << ": " << operands_[i]->GetType()->ToString();
      if (i != operands_.size() - 1) {
        *out << ", ";
      }
    }
    *out << ") ";
    if (IsLibraryFunction(name_)) {
      *out << "(lib)\n";
      return;
    }
    *out << "{\n";
    IncreaseIndent();
    // Emit the body of the function
    for (auto region : regions_) {
      region->indent_level_ = indent_level_;
      region->Emit(out);
    }
    DecreaseIndent();
    PrintIndent(out);
    *out << "}\n";
  }

  auto Verify(context::CompilerContext *context) -> bool override;

  /**
   * @brief Interface to get the name of the function
   */
  auto GetName() const -> std::string { return name_; }

  /**
   * @brief Interface to get the return type of the function
   */
  auto GetReturnType() const -> type::Type * { return results_[0]->GetType(); }

  /**
   * @brief Interface to get the arguments of the function
   */
  auto GetArguments() const -> std::vector<mlir::Value *> { return operands_; }

  /**
   * @brief Interface to get the return value
   */
  auto GetRes() -> mlir::Value * { return results_[0]; }

  /**
   * @brief Get the entry block
   */
  auto GetEntryBlock() -> middleend::mlir::Block * {
    if (regions_.empty() || regions_[0]->blocks_.empty()) {
      throw std::runtime_error("The function definition operation has no entry block.");
    }
    return regions_[0]->blocks_[0];
  }

  auto GetBlocks() -> std::vector<middleend::mlir::Block *> {
    if (regions_.empty()) {
      throw std::runtime_error("The function definition operation has no blocks.");
    }
    return regions_[0]->blocks_;
  }

 private:
  FuncDefOp() : BaseOps("funcdef"){};

  auto SetUp(mlir::Value *res, const std::string &name, std::vector<mlir::Value *> args) -> void {
    // Used in lowering, the region and block have already been generated in the original FuncDefOp, so we just need to
    // set the operands and results
    name_ = name;
    operands_ = std::move(args);
    results_.emplace_back(res);
    res->op_ = this;

    // Create entry block
    regions_.emplace_back(new Region());
    auto entry_block = new Block();
    entry_block->block_name_ = name_ + "_entry";
    regions_[0]->blocks_.emplace_back(entry_block);
  };

  std::string name_;
};

}  // namespace middleend::mlir::base

#endif