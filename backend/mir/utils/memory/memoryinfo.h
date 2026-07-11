#ifndef BACKEND_MIR_UTILS_MEMORY_MEMORYINFO_H
#define BACKEND_MIR_UTILS_MEMORY_MEMORYINFO_H

#include <cstddef>
#include <string>
namespace backend::utils {
class MemoryInfo {
  /**
   * @brief The MemoryInfo class will record the alignment and size information
   */
 public:
  size_t size_;
  size_t align_;

  // Used in debugging, output the memory info
  auto ToString() const -> std::string {
    std::string result = "size: " + std::to_string(size_) + ", align: " + std::to_string(align_);
    return result;
  }
};

}  // namespace backend::utils

#endif