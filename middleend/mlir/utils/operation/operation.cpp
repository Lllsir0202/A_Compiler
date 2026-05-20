#include "middleend/mlir/utils/operation/operation.h"
#include "middleend/mlir/utils/adapter/adapter.h"
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

auto ModuleOp::GetBlock() -> Block * {
  if (regions_.empty()) {
    throw std::runtime_error("Module operation has no region.");
  }
  if (regions_[0]->blocks_.empty()) {
    throw std::runtime_error("Region has no block.");
  }
  return regions_[0]->blocks_[0];
}

auto ModuleOp::GetRegion() -> Region * {
  if (regions_.empty()) {
    throw std::runtime_error("Module operation has no region.");
  }
  return regions_[0];
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

auto ModuleOp::Verify(context::CompilerContext *context) -> bool {
  // Verify the module operation
  // First the module operation should have only one region and one block
  if (regions_.size() != 1) {
    return false;
  }
  auto region = regions_[0];
  if (region->blocks_.size() != 1) {
    return false;
  }
  // Then verify the operations in the block
  auto block = region->blocks_[0];
  for (auto op : block->operations_) {
    if (!op->Verify(context)) {
      return false;  // If any operation is not valid, the module operation is not valid
    }
  }
  return true;  // If all operations are valid, the module operation is valid
}

auto ModuleOp::Reset() -> void {
  // Reset the state of the module operation, such as the blocks and regions, etc.
  for (auto region : regions_) {
    delete region;
  }
  regions_.clear();
  operands_.clear();
  results_.clear();
}

auto Operation::Walk(Adapter *adapter) -> void {
  // Walk the operation, which means we will call the adapter to adapt the operation
  // and then we will walk the regions of this operation
  adapter->Adapt(this);
}

}  // namespace middleend::mlir