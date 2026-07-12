#ifndef MIDDLEEND_LLVM_INST_MEMORY_ALLOCAINST_H
#define MIDDLEEND_LLVM_INST_MEMORY_ALLOCAINST_H
#include <utility>

#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class AllocaInst : public Instruction {
 public:
  // Interfaces to get alloca information
  auto GetAllocatedType() const -> type::Type * { return allocated_type_; }

  // Override the Emit and Verify functions
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  // Interface to get the dimensions of the allocated array
  auto GetDimensions() const -> const std::vector<int> & { return dimensions_; }

 private:
  explicit AllocaInst(type::Type *allocated_type, std::vector<int> dimensions)
      : Instruction(OpKind::Alloca), allocated_type_(allocated_type), dimensions_(std::move(dimensions)) {}
  static auto Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                     std::vector<int> dimensions) -> AllocaInst *;
  friend class utils::factory::InstFactory;
  // Type of the allocated memory
  type::Type *allocated_type_;

  // Since we only support alloca for array type,
  // we need to record the dimension of the array
  std::vector<int> dimensions_;
};

}  // namespace middleend::llvm::inst

#endif