#include "backend/mir/utils/struct/function.h"
#include "backend/mir/utils/factory/functionfactory.h"
#include "backend/mir/utils/memory/frameinfo.h"
#include "backend/mir/utils/struct/block.h"
namespace backend::utils {
Function::~Function() {
  for (auto *block : blocks_) {
    delete block;
  }
  blocks_.clear();
  for (auto *frame_info : frame_infos_) {
    delete frame_info;
  }
  frame_infos_.clear();
}

auto Function::Emit(std::ofstream *out) const -> void {
  *out << "{\n";
  for (size_t i = 0; i < blocks_.size(); ++i) {
    auto block = blocks_[i];
    block->Emit(out);
    if (i != blocks_.size() - 1) {
      *out << "\n";
    }
  }
  *out << "}\n";
}

auto Function::GetFrameInfoById(size_t frame_id) const -> FrameInfo * {
  if (frame_id >= frame_infos_.size()) {
    return nullptr;
  }
  return frame_infos_[frame_id];
}

auto Function::AddFrameInfo(size_t size, size_t align) -> FrameInfo * {
  auto *frame_info = factory::FunctionFactory::CreateFrameInfo(frame_infos_.size());
  frame_info->SetSize(size);
  frame_info->SetAlign(align);
  frame_infos_.emplace_back(frame_info);
  return frame_info;
}

}  // namespace backend::utils