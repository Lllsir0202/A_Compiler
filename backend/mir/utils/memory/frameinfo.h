#ifndef BACKEND_MIR_UTILS_MEMORY_FRAMEINFO_H
#define BACKEND_MIR_UTILS_MEMORY_FRAMEINFO_H

#include <cstddef>
namespace backend::utils::factory {
class FunctionFactory;
}
namespace backend::utils {
class FrameInfo {
 public:
  ~FrameInfo() = default;
  auto GetFrameId() const -> size_t { return frame_id_; }
  auto GetSize() const -> size_t { return size_; }
  auto SetSize(size_t size) -> void { size_ = size; }
  auto GetAlign() const -> size_t { return align_; }
  auto SetAlign(size_t align) -> void { align_ = align; }

 private:
  explicit FrameInfo(size_t frame_id) : frame_id_(frame_id) {}
  const size_t frame_id_;

  // Record the size of the stack frame,
  // which is the total size of the slot
  size_t size_ = 0;
  // alignment of the stack
  size_t align_ = 0;

  // DO NOT allow offset,since it will be decided after register allocation,
  // and it is not the concern of the MIR.
  friend class factory::FunctionFactory;
};
}  // namespace backend::utils
#endif