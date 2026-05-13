#ifndef MIDDLEEND_LLVM_INST_MEMORY_GLOBALDEF_H
#define MIDDLEEND_LLVM_INST_MEMORY_GLOBALDEF_H
#include <utility>

#include "middleend/llvm/inst/inst.h"
#include "utils/type/type.h"
namespace middleend::llvm::inst {
class GlobalDefInst : public Instruction {
 public:
  auto Emit(std::ofstream *out) const -> void override;
  auto Verify(context::CompilerContext *context) const -> bool override;

  // Interface to get the global variable information, such as the name, type, and initial value
  auto GetName() const -> const std::string & { return name_; }
  auto GetAllocatedType() const -> type::Type * { return allocated_type_; }
  auto GetIntInitialValues() const -> const std::vector<int> & { return int_initial_values_; }
  auto GetFloatInitialValues() const -> const std::vector<double> & { return float_initial_values_; }
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
                         std::vector<double> initial_values)
      : Instruction(OpKind::GlobalDef),
        allocated_type_(allocated_type),
        dimensions_(std::move(dimensions)),
        name_(std::move(name)),
        float_initial_values_(std::move(initial_values)) {}

  static auto Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                     const std::string &name, std::vector<int> dimensions,
                     std::vector<int> initial_values) -> GlobalDefInst *;
  static auto Create(context::LLVMIRContext *context, type::Type *result_type, type::Type *allocated_type,
                     const std::string &name, std::vector<int> dimensions,
                     std::vector<double> initial_values) -> GlobalDefInst *;
  friend class utils::factory::InstFactory;
  type::Type *allocated_type_;
  std::vector<int> dimensions_;
  std::string name_;

  std::vector<int> int_initial_values_;
  std::vector<double> float_initial_values_;
};
}  // namespace middleend::llvm::inst
#endif