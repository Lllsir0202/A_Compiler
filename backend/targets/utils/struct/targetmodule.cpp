#include "backend/targets/utils/struct/targetmodule.h"

#include <cstring>

namespace backend::targetir {
namespace {
auto FloatToBits(float value) -> std::uint32_t {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float and uint32_t size mismatch");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}
}  // namespace

TargetModule::~TargetModule() {
  for (auto function : functions_) {
    delete function;
  }
  functions_.clear();
  for (auto global : global_objects_) {
    delete global;
  }
  global_objects_.clear();
  library_functions_.clear();
  float_literal_pool_.clear();
  float_literal_pool_index_.clear();
}

auto TargetModule::GetOrCreateFloatLiteralLabel(float value) -> std::string {
  const auto bits = FloatToBits(value);
  if (auto it = float_literal_pool_index_.find(bits); it != float_literal_pool_index_.end()) {
    return float_literal_pool_[it->second].GetLabel();
  }

  auto label = ".LCPI" + std::to_string(float_literal_pool_.size());
  float_literal_pool_index_[bits] = float_literal_pool_.size();
  float_literal_pool_.emplace_back(label, bits);
  return label;
}

auto TargetModule::Emit(std::ofstream *out) -> void {
  for (auto function : functions_) {
    function->Emit(out);
  }

  *out << "\n";

  for (auto global : global_objects_) {
    global->Emit(out);
    *out << "\n";
  }

  if (!float_literal_pool_.empty()) {
    *out << "    .section    .rodata\n";
    for (const auto &literal : float_literal_pool_) {
      *out << "    .p2align    2\n";
      *out << literal.GetLabel() << ":\n";
      *out << "    .word    " << literal.GetBits() << "\n";
    }
    *out << "\n";
  }

  // In the end, emit the global variables and library functions
  if (!library_functions_.empty()) {
    *out << "    .addrsig\n";
  }
  for (auto &lib_func : library_functions_) {
    *out << "    .addrsig_sym " << lib_func;
    if (lib_func != library_functions_.back()) {
      *out << "\n";
    }
  }
}

}  // namespace backend::targetir
