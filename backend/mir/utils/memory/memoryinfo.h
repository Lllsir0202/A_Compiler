#ifndef BACKEND_MIR_UTILS_MEMORY_MEMORYINFO_H
#define BACKEND_MIR_UTILS_MEMORY_MEMORYINFO_H

#include <cstddef>
namespace backend::utils {
class MemoryInfo {
  /**
   * @brief The MemoryInfo class will record the alignment and size information
   */
 public:
  size_t size_;
  size_t align_;
};

}  // namespace backend::utils

#endif