#include "backend/mir/inst/memory/globaldef.h"

#include <utility>
#include "context/compiler_context.h"

namespace backend::inst {
auto GlobalDefInst::Verify(context::CompilerContext *context) const -> bool {
  // Check if the allocated type is valid
  if (allocated_type_ == nullptr) {
    context->Print("Global variable " + name_ + " has null allocated type");
    return false;
  }
  return true;
}

auto GlobalDefInst::Create(context::MIRContext *context, type::Type *result_type, type::Type *allocated_type,
                           const std::string &name, std::vector<int> dimensions, std::vector<int> initial_values,
                           bool is_constant) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, name, std::move(dimensions), std::move(initial_values));
  inst->is_constant_ = is_constant;
  auto res = context->GetGlobalOperand(name);
  res->SetType(result_type);
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto GlobalDefInst::Create(context::MIRContext *context, type::Type *result_type, type::Type *allocated_type,
                           const std::string &name, std::vector<int> dimensions, std::vector<float> initial_values,
                           bool is_constant) -> GlobalDefInst * {
  auto *inst = new GlobalDefInst(allocated_type, name, std::move(dimensions), std::move(initial_values));
  inst->is_constant_ = is_constant;
  auto res = context->GetGlobalOperand(name);
  res->SetType(result_type);
  inst->SetResult(res);
  res->SetDefInst(inst);
  return inst;
}

auto GlobalDefInst::Emit(std::ofstream *out) const -> void {
  *out << "GlobalDef " << name_ << " : " << allocated_type_->ToString();
  if (!dimensions_.empty()) {
    *out << " [";
    for (size_t i = 0; i < dimensions_.size(); ++i) {
      *out << dimensions_[i];
      if (i != dimensions_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "]";
  }
  if (!int_initial_values_.empty()) {
    *out << " = [";
    for (size_t i = 0; i < int_initial_values_.size(); ++i) {
      *out << int_initial_values_[i];
      if (i != int_initial_values_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "]";
  } else if (!float_initial_values_.empty()) {
    *out << " = [";
    for (size_t i = 0; i < float_initial_values_.size(); ++i) {
      *out << float_initial_values_[i];
      if (i != float_initial_values_.size() - 1) {
        *out << ", ";
      }
    }
    *out << "]";
  }
  *out << "\n";
}

}  // namespace backend::inst