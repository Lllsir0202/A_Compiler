#include "backend/targets/rv64/symbol/rv64global.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace backend::targetir {
namespace {
auto FloatToBits(float value) -> std::uint32_t {
  std::uint32_t bits = 0;
  static_assert(sizeof(bits) == sizeof(value), "float and uint32_t size mismatch");
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}
}  // namespace

auto RV64GlobalObject::GetAlignmentLog2() const -> size_t {
  size_t align = GetAlignment();
  size_t log2 = 0;
  while ((static_cast<size_t>(1) << log2) < align) {
    ++log2;
  }
  return log2;
}

auto RV64GlobalObject::EmitInitializer(std::ofstream *out) const -> void {
  size_t element_count = 1;
  for (auto dim : GetDimensions()) {
    element_count *= static_cast<size_t>(dim);
  }
  if (GetDimensions().empty()) {
    element_count = 1;
  }

  if (!GetIntInitialValues().empty()) {
    for (size_t i = 0; i < element_count; ++i) {
      const auto value = i < GetIntInitialValues().size() ? GetIntInitialValues()[i] : 0;
      *out << "    .word    " << value << "\n";
    }
    return;
  }

  if (!GetFloatInitialValues().empty()) {
    for (size_t i = 0; i < element_count; ++i) {
      const auto value = i < GetFloatInitialValues().size() ? GetFloatInitialValues()[i] : 0.0F;
      *out << "    .word    " << FloatToBits(value) << "\n";
    }
    return;
  }

  *out << "    .zero    " << GetSize() << "\n";
}

auto RV64GlobalObject::Emit(std::ofstream *out) const -> void {
  if (GetAllocatedType() == nullptr) {
    throw std::runtime_error("RV64GlobalObject has null allocated type");
  }

  *out << "    .type    " << GetName() << ",@object\n";
  if (IsConstant()) {
    *out << "    .section    .rodata\n";
  } else if (IsZeroInitialized()) {
    *out << "    .bss\n";
  } else {
    *out << "    .data\n";
  }
  *out << "    .globl    " << GetName() << "\n";
  *out << "    .p2align    " << GetAlignmentLog2() << ", 0x0\n";
  *out << GetName() << ":\n";
  EmitInitializer(out);
  *out << "    .size    " << GetName() << ", " << GetSize() << "\n";
}

}  // namespace backend::targetir
