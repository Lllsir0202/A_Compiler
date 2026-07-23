#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFUNCTION_H
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "targets/utils/manager/registerpool.h"
#include "targets/utils/operand/targetreg.h"
#include "targets/utils/struct/literalpoolentry.h"

namespace backend::targetir {
class TargetBlock;
class TargetOperand;
class TargetInst;
class TargetFrameObject;

class TargetFunction {
 public:
  // Destructor: delete all blocks
  ~TargetFunction();
  auto Emit(std::ofstream *out) -> void;
  auto AddBlock(TargetBlock *block) -> void { blocks_.emplace_back(block); }
  auto GetBlocks() const -> const std::vector<TargetBlock *> & { return blocks_; }
  auto GetName() const -> const std::string & { return name_; }
  [[maybe_unused]] auto SetBlocks(const std::vector<TargetBlock *> &blocks) -> void { blocks_ = blocks; }
  [[maybe_unused]] auto ReplaceBlock(TargetBlock *old_block, TargetBlock *new_block) -> void {
    for (auto &block : blocks_) {
      if (block == old_block) {
        block = new_block;
      }
    }
  }

  // Interface to get, add frame objects
  auto GetFrameObjects() const -> const std::vector<TargetFrameObject *> & { return frame_objects_; }
  auto GetFrameObjectByID(size_t id) -> TargetFrameObject * {
    if (id < frame_objects_.size()) {
      return frame_objects_[id];
    }
    return nullptr;
  }
  auto GetFrameObjectsSize() const -> size_t { return frame_objects_.size(); }
  auto AddFrameObject(TargetFrameObject *frame_object) -> void { frame_objects_.emplace_back(frame_object); }
  auto ReserveFrameObject(size_t size) -> void { frame_objects_.reserve(size); }

  auto GetVirtualRegisters() -> std::vector<TargetVirtualReg *> & { return virtual_registers_; }
  auto AddVirtualRegister(TargetVirtualReg *virtual_register) -> void {
    virtual_registers_.emplace_back(virtual_register);
  }
  auto GetVirtualRegisterByID(size_t id) -> TargetVirtualReg * {
    if (id < virtual_registers_.size()) {
      return virtual_registers_[id];
    }
    return nullptr;
  }

  // After counting the frequency of float literal usage,
  // we can remain only those float literals
  // whose usage count is greater than a specified threshold.
  // Then we can record those frequently used float literals
  auto RecordFrequentlyUsedFloatLiterals(size_t threshold) -> void {
    for (const auto &[literal, count] : float_literal_usage_count_) {
      if (count >= threshold) {
        frequently_used_float_literals_.insert(literal);
      }
    }
    // Clear the usage count map to save memory
    float_literal_usage_count_.clear();
  }

  // Provide an interface to update the usage count of a float literal value.
  auto UpdateFloatLiteralUsageCount(float literal_value) -> void {
    if (float_literal_usage_count_.count(literal_value) == 0) {
      float_literal_usage_count_[literal_value] = 1;
    } else {
      float_literal_usage_count_[literal_value]++;
    }
  }

  // Record whether a float literal value is frequently used.
  auto IsFloatLiteralFrequentlyUsed(float literal_value) const -> bool {
    return frequently_used_float_literals_.count(literal_value) > 0;
  }

  auto GetOperandForFloatLiteral(float literal_value) -> TargetVirtualReg * {
    if (float_literal_operand_map_.count(literal_value) > 0) {
      return float_literal_operand_map_[literal_value];
    }
    return nullptr;
  }

  auto SetOperandForFloatLiteral(float literal_value, TargetVirtualReg *operand) -> void {
    float_literal_operand_map_[literal_value] = operand;
  }

  // Update and get the float literal pool entry for a float literal value.
  auto UpdateFloatLiteralPoolEntry(float literal_value, size_t offset, size_t index) -> void {
    float_literal_pool_[literal_value] = TargetFloatLiteralPoolEntry(literal_value, offset, index);
  }

  auto GetFloatLiteralPoolEntry(float literal_value) -> TargetFloatLiteralPoolEntry * {
    if (float_literal_pool_.count(literal_value) > 0) {
      return &float_literal_pool_[literal_value];
    }
    return nullptr;
  }

  auto GetFloatLiteralPoolSize() const -> size_t { return float_literal_pool_.size(); }

  auto GetOffsetOfLocalFrameObject() const -> size_t { return offset_of_local_frame_object_; }
  auto SetOffsetOfLocalFrameObject(size_t offset) -> void { offset_of_local_frame_object_ = offset; }
  auto IncreaseOffsetOfLocalFrameObject(size_t offset) -> void { offset_of_local_frame_object_ += offset; }

 private:
  // Constructor with function name
  explicit TargetFunction(std::string name) : name_(std::move(name)) {}
  friend class FunctionFactory;
  // Record a list of blocks
  std::vector<TargetBlock *> blocks_;

  // A list of frame objects
  std::vector<TargetFrameObject *> frame_objects_;
  // Function name
  std::string name_;

  // Record the frameobject which has import the parameter

  /**
   * @brief The offset of the local frame object, used in lowering.
   * Stack frame layout:
   * High address
   * |---------------------|
   * |  Return address     |
   * |---------------------|
   * |  Saved registers    |
   * |---------------------|
   * |  Local frame object |
   * |---------------------|
   * |  Parameter frame    |
   * |  object             |
   * |---------------------|
   * Low address
   */
  size_t offset_of_local_frame_object_ = 0;

  // Record all virtual registers in this function, which will be used for register allocation.
  // Note that the index of the virtual register in this vector is its id.
  std::vector<TargetVirtualReg *> virtual_registers_;

  // Record all physical registers has been used in this function,
  // which will be used for register allocation.
  std::unordered_set<PhysicalReg *> used_physical_registers_;

  // Record float literal pool, which will be used to store the float literal values
  std::unordered_map<float, TargetFloatLiteralPoolEntry> float_literal_pool_;

  // Record the frequently used float literal values, which will be used to decide whether to use li instruction or
  // load from literal pool.
  std::unordered_set<float> frequently_used_float_literals_;

  // Record whether a literal value is frequently used,
  // which will be used to decide whether to use li instruction or load from literal pool.
  std::unordered_map<float, size_t> float_literal_usage_count_;

  // Record whether a literal value is already mapped to a target operand,
  // Note that this is only used for frequently used float literal values.
  std::unordered_map<float, TargetVirtualReg *> float_literal_operand_map_;
};

}  // namespace backend::targetir

#endif