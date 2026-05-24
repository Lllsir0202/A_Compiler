#ifndef BACKEND_MIR_INST_MEMORY_GLOBALDEF_H
#define BACKEND_MIR_INST_MEMORY_GLOBALDEF_H

#include <string>
#include "backend/mir/inst/inst.h"
#include "utils/type/type.h"

namespace backend::inst {
class GlobalDefInst : public Instruction {
 public:
  auto Verify(context::CompilerContext *context) const -> bool override;

  // Interface to get the global variable information, such as the name, type, and initial value
  auto GetName() const -> const std::string & { return name_; }
  auto GetAllocatedType() const -> type::Type * { return allocated_type_; }
  auto GetIntInitialValues() const -> const std::vector<int> & { return int_initial_values_; }
  auto GetFloatInitialValues() const -> const std::vector<float> & { return float_initial_values_; }
  auto GetDimensions() const -> const std::vector<int> & { return dimensions_; }

 private:
  explicit GlobalDefInst(type::Type *allocated_type, std::string name, std::vector<int> dimensions,
                         std::vector<int> initial_values)
      : Instruction(OpKind::GlobalDef),
        allocated_type_(allocated_type),
        dimensions_(std::move(dimensions)),
        name_(std::move(name)),
        int_initial_values_(std::move(initial_values)) {}
  explicit GlobalDefInst(type::Type *allocated_type, std::string name, std::vector<int> dimensions,
                         std::vector<float> initial_values)
      : Instruction(OpKind::GlobalDef),
        allocated_type_(allocated_type),
        dimensions_(std::move(dimensions)),
        name_(std::move(name)),
        float_initial_values_(std::move(initial_values)) {}

  static auto Create(context::MIRContext *context, type::Type *result_type, type::Type *allocated_type,
                     const std::string &name, std::vector<int> dimensions, std::vector<int> initial_values,
                     bool is_constant = false) -> GlobalDefInst *;
  static auto Create(context::MIRContext *context, type::Type *result_type, type::Type *allocated_type,
                     const std::string &name, std::vector<int> dimensions, std::vector<float> initial_values,
                     bool is_constant = false) -> GlobalDefInst *;
  friend class utils::factory::InstFactory;
  type::Type *allocated_type_;
  std::vector<int> dimensions_;
  std::string name_;

  std::vector<int> int_initial_values_;
  std::vector<float> float_initial_values_;
  bool is_constant_;
};

}  // namespace backend::inst

#endif