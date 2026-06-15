#ifndef BACKEND_TARGETS_UTILS_RA_METHOD_ALLOCATOR_H
#define BACKEND_TARGETS_UTILS_RA_METHOD_ALLOCATOR_H

namespace backend::targetir {
class RegisterAllocator {
 public:
  virtual ~RegisterAllocator() = default;

  // Allocate registers for the current function, and return the mapping from virtual registers to physical registers
  virtual auto Allocate() -> void = 0;
};

}  // namespace backend::targetir
#endif