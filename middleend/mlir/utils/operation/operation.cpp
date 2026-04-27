#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/struct/block.h"
#include "middleend/mlir/utils/struct/region.h"
namespace middleend::mlir {
Operation::~Operation() {
  /**
   * @brief The destructor of the operation
   * it will delete all the regions of this operation
   * the value destructor will be called separately
   * since the value might be used by other operations
   */
  for (auto region : regions_) {
    delete region;
  }
}

/**
 * @brief Used for formating the output of the operation
 */
auto Operation::PrintIndent(std::ofstream *out) const -> void {
  for (int i = 0; i < indent_level_; ++i) {
    *out << "  ";
  }
}

/**
 * @brief This function will be used to set up the module operation
 * which means it will alloca a region and then create a block in the region
 * and then the following operations will be inserted into the block
 */
auto ModuleOp::SetUp() -> void {
  auto *region = new Region();
  auto *block = new Block();
  region->blocks_.emplace_back(block);
  regions_.emplace_back(region);
}

auto ModuleOp::Emit(std::ofstream *out) -> void {
  /**
   * @brief A moduleop is the top-level operation in a program
   * i.e.
   * builtin.module {
   *    region {
   *       block(arguments) {
   *          op1(i.e. funcop)
   *          op2(i.e. globalvariableop)
   *       }
   * }
   */
  PrintIndent(out);
  *out << this->Name() << " {\n";
  IncreaseIndent();
  for (auto region : regions_) {
    region->indent_level_ = indent_level_;
    region->Emit(out);
  }
  DecreaseIndent();
  PrintIndent(out);
  *out << "}\n";
}

}  // namespace middleend::mlir