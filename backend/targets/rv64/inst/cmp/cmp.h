#ifndef BACKEND_TARGETS_RV64_INST_CMP_CMP_H
#define BACKEND_TARGETS_RV64_INST_CMP_CMP_H

#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
// ICmp
class RVSLTInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLTInst *;
  friend class InstFactory;
};

class RVSLEInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLEInst *;
  friend class InstFactory;
};

class RVSEQInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSEQInst *;
  friend class InstFactory;
};

// FCmp
class RVFLTSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLTSInst *;
  friend class InstFactory;
};

class RVFLESInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLESInst *;
  friend class InstFactory;
};

class RVFEQSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFEQSInst *;
  friend class InstFactory;
};

}  // namespace backend::targetir::rv64

#endif