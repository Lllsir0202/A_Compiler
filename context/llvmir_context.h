#ifndef CONTEXT_LLVMIR_CONTEXT_H
#define CONTEXT_LLVMIR_CONTEXT_H
#include <unordered_map>
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/llvm/utils/operand/regoperand.h"
#include "context/type_context.h"
namespace context {

/**
 * @brief LLVMIRLowerContext is responsible for lowering the base ir to llvmir
 */
class LLVMIRLowerContext {
  public:

  private:

};

/**
 * @brief LLVMIRResourceContext is responsible for managing the resources of llvm ir.
 */
class LLVMIRResourceContext {
 public:
  /**
   * @brief Interfaces to create new operands
   */
  auto GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand *;

  auto GetNewLabelOperand() -> middleend::llvm::utils::LabelOperand *;

  auto GetNewIntLiteral(int value, type::Type *type) -> middleend::llvm::utils::IntLiteral *;

  auto GetNewFloatLiteral(float value, type::Type *type) -> middleend::llvm::utils::FloatLiteral *;

  /**
   * @brief Interfaces to get existing operands by id(or value for literals),
   * to ensure that the same operand will always be the same instance of operand.
   */
  auto GetRegOperand(size_t id) -> middleend::llvm::utils::RegOperand *;
  auto GetLabelOperand(size_t id) -> middleend::llvm::utils::LabelOperand *;
  auto GetIntLiteral(int value) -> middleend::llvm::utils::IntLiteral *;
  auto GetFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral *;

 private:
  // Id counters for different operand types
  size_t reg_id_counter_ = 0;
  size_t label_id_counter_ = 0;

  // Map from id to operand instance, to ensure that the same operand will be mapped to the same operand
  std::unordered_map<size_t, middleend::llvm::utils::RegOperand *> reg_operands_;
  std::unordered_map<size_t, middleend::llvm::utils::LabelOperand *> label_operands_;
  std::unordered_map<int, middleend::llvm::utils::IntLiteral *> int_literals_;
  std::unordered_map<float, middleend::llvm::utils::FloatLiteral *> float_literals_;
};

class LLVMIRContext {
 public:
  explicit LLVMIRContext(TypeContext *type_context) : type_context_(type_context) {
    resource_context_ = new LLVMIRResourceContext();
    lower_context_ = new LLVMIRLowerContext();
  }
  // Get different types of operands from the context,
  // and we will ensure that the same operand will always be the same instance of operand.
  auto GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand * {
    return resource_context_->GetNewRegOperand(type);
  }

  auto GetNewLabelOperand() -> middleend::llvm::utils::LabelOperand * {
    return resource_context_->GetNewLabelOperand();
  }

  auto GetNewIntLiteral(int value) -> middleend::llvm::utils::IntLiteral * {
    return resource_context_->GetNewIntLiteral(value, type_context_->GetIntegerType());
  }

  auto GetNewFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral * {
    return resource_context_->GetNewFloatLiteral(value, type_context_->GetFloatType());
  }

  // Get types
  auto GetIntegerType() -> type::Type * { return type_context_->GetIntegerType(); }
  auto GetFloatType() -> type::Type * { return type_context_->GetFloatType(); }
  auto GetBoolType() -> type::Type * { return type_context_->GetBooleanType(); }

 private:
  [[maybe_unused]] LLVMIRLowerContext *lower_context_;
  LLVMIRResourceContext *resource_context_;

  TypeContext *type_context_;
};

}  // namespace context
#endif