#ifndef BACKEND_TARGETS_RV64_INST_CAST_CAST_H
#define BACKEND_TARGETS_RV64_INST_CAST_CAST_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {

// fcvt.s.w rd rs1, cast rs1 to float and store in rd
// INT to FLOAT
class RVFCVTSWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFCVTSWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTSWInst *;
  friend class InstFactory;
};

// fcvt.w.s rd rs1, cast rs1 to int and store in rd
// FLOAT to INT
class RVFCVTWSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFCVTWSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTWSInst *;
  friend class InstFactory;
};

// fcvt.s.wu rd rs1, cast rs1 to float and store in rd
// UINT to FLOAT
class RVFCVTSWUInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFCVTSWUInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> RVFCVTSWUInst *;
  friend class InstFactory;
};

// zext.w rd rs1, zero-extend the lower 32 bits of rs1 and store in rd
// BOOL to INT
class RVZextWInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVZextWInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1) -> RVZextWInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif