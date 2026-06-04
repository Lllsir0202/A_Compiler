#ifndef BACKEND_MIR_UTILS_OPERAND_LABELOPERAND_H
#define BACKEND_MIR_UTILS_OPERAND_LABELOPERAND_H
#include "backend/mir/utils/operand/operand.h"
namespace backend::utils {
class LabelOperand : public Operand {
 public:
  auto Emit(std::ofstream *out) const -> void override {
    // The label operand will be emitted as the label id
    *out << "label_" << id_;
  }
  auto GetId() const -> size_t { return id_; }

 private:
  explicit LabelOperand(size_t id) : Operand(OpType::Label), id_(id) {}
  size_t id_;
  friend class factory::OperandFactory;
};

}  // namespace backend::utils

#endif