#ifndef BACKEND_TARGETS_RV64_INST_CMP_CMP_H
#define BACKEND_TARGETS_RV64_INST_CMP_CMP_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
// ICmp
class RVSLTInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLTInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLTInst *;
  friend class InstFactory;
};

class RVSLEInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLEInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLEInst *;
  friend class InstFactory;
};

class RVSEQInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSEQInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSEQInst *;
  friend class InstFactory;
};

// FCmp
class RVFLTSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLTSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLTSInst *;
  friend class InstFactory;
};

class RVFLESInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLESInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLESInst *;
  friend class InstFactory;
};

class RVFEQSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFEQSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFEQSInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif