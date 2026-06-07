#ifndef BACKEND_TARGETS_UTILS_STRUCT_TARGETFRAMEOBJECT_H
#define BACKEND_TARGETS_UTILS_STRUCT_TARGETFRAMEOBJECT_H

#include <cstddef>
namespace backend::targetir {
// TODO(loki): Might need change
class TargetFrameObject {
 public:
  auto GetID() -> size_t { return id_; }
  auto GetSize() -> size_t { return size_; }
  auto GetAlignment() -> size_t { return alignment_; }
  auto GetOffset() -> size_t { return offset_; }
  auto SetOffset(size_t offset) -> void { offset_ = offset; }

 private:
  explicit TargetFrameObject(size_t id, size_t size, size_t alignment) : id_(id), size_(size), alignment_(alignment) {}

  // Frame object id, used to identify the frame object in the stack frame
  size_t id_;
  // Size of the frame object in bytes
  size_t size_;
  // Alignment
  size_t alignment_;
  // offset in the stack frame, but not decided initially.
  size_t offset_;
};

}  // namespace backend::targetir

#endif