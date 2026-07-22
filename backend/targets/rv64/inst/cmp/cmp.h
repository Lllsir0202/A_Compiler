#ifndef BACKEND_TARGETS_RV64_INST_CMP_CMP_H
#define BACKEND_TARGETS_RV64_INST_CMP_CMP_H

#include "backend/targets/utils/factory/instfactory.h"
#include "backend/targets/utils/inst/targetinst.h"
namespace backend::targetir::rv64 {
// ICmp
class RVSLTInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLTInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLTInst *;
  friend class targetir::InstFactory;
};

class RVSLEInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLEInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVSLEInst *;
  friend class targetir::InstFactory;
};

// Immediate Comparsion Instructions
class RVSLTIInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVSLTIInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *imm) -> RVSLTIInst *;
  friend class targetir::InstFactory;
};

// FCmp
class RVFLTSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLTSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLTSInst *;
  friend class targetir::InstFactory;
};

class RVFLESInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFLESInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFLESInst *;
  friend class targetir::InstFactory;
};

class RVFEQSInst : public TargetInst {
 public:
  auto Emit(std::ofstream *out) -> void override;

 private:
  RVFEQSInst() = default;
  static auto Create(TargetOperand *rd, TargetOperand *rs1, TargetOperand *rs2) -> RVFEQSInst *;
  friend class targetir::InstFactory;
};

}  // namespace backend::targetir::rv64

#endif