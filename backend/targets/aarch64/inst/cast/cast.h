#ifndef BACKEND_TARGETS_AARCH64_INST_CAST_CAST_H
#define BACKEND_TARGETS_AARCH64_INST_CAST_CAST_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::aarch64 {

// scvtf: convert integer to float
class ARMSCVTFInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> ARMSCVTFInst *;
  friend class InstFactory;
};

// ucvtf: convert unsigned integer to float
class ARMUCVTFInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMUCVTFInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> ARMUCVTFInst *;
  friend class InstFactory;
};

// fcvtzs: convert float to integer, round towards zero
class ARMFCVTZSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  ARMFCVTZSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> ARMFCVTZSInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::aarch64

#endif