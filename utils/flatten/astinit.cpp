#include "utils/flatten/astinit.h"
#include <cstdio>

namespace initializer {
auto ConstInitializer::SetIntValue(int value) -> void {
  int_value_ = value;
  float_value_ = std::nullopt;
}

auto ConstInitializer::SetFloatValue(double value) -> void {
  float_value_ = value;
  int_value_ = std::nullopt;
}

/**
* @brief Even though we do not check
* whether the initializer is the specific type, it will still be nullopt,
* since we will only set one type of value in the initializer, so we can just return the value directly without checking
the type.
*/
auto ConstInitializer::GetIntValue() const -> std::optional<int> { return int_value_; }

auto ConstInitializer::GetFloatValue() const -> std::optional<double> { return float_value_; }

/**
 * @brief Handle the initializer in the AST, which is used for constant expression evaluation
 */
auto HandleInAst(frontend::ast::Initializer *init_node) -> initializer::InitializerPtr {
  auto const_init = std::make_unique<ConstInitializer>();
  if (init_node == nullptr) {
    // means there is default initializer
    return const_init;
  }
  if (init_node->IsScalar()) {
    auto scalar_init = static_cast<frontend::ast::InitValNode *>(init_node);
    if (scalar_init->value_->GetType()->GetKind() == type::TypeKind::INT) {
      const_init->SetIntValue(scalar_init->value_->GetValue().GetIntValue());
    } else if (scalar_init->value_->GetType()->GetKind() == type::TypeKind::FLOAT) {
      const_init->SetFloatValue(scalar_init->value_->GetValue().GetFloatValue());
    } else if (scalar_init->value_->GetType()->GetKind() == type::TypeKind::BOOL) {
      const_init->SetIntValue(scalar_init->value_->GetValue().GetBoolValue() ? 1 : 0);
    } else {
      throw std::logic_error("Unsupported scalar initializer type");
    }
  } else {
    throw std::logic_error("Non-scalar initializer cannot be handled in constant expression evaluation");
  }
  return const_init;
}

auto CheckInAst(frontend::ast::Initializer *init_node) -> initializer::InitializerPtr {
  // In this function, we just check whether the initializer is castable to the specific type
  if (init_node == nullptr) {
    // means there is default initializer
    return nullptr;
  }
  if (init_node->IsScalar()) {
    auto scalar_init = static_cast<frontend::ast::InitValNode *>(init_node);
    if (scalar_init->value_->GetType()->GetKind() == type::TypeKind::INT ||
        scalar_init->value_->GetType()->GetKind() == type::TypeKind::FLOAT ||
        scalar_init->value_->GetType()->GetKind() == type::TypeKind::BOOL) {
      return nullptr;
    }
    throw std::logic_error("Unsupported scalar initializer type");
  }
  throw std::logic_error("Non-scalar initializer cannot be handled in constant expression evaluation");
}

/**
 * @brief Helper function , which is used to get the value of the initializer directly
 * used as a wrapper to pass value to the frontend::ast::Value class
 */
auto ConstInitializer::GetValue() const -> frontend::ast::Value {
  if (int_value_.has_value()) {
    return frontend::ast::Value(int_value_.value());
  }
  if (float_value_.has_value()) {
    return frontend::ast::Value(float_value_.value());
  }
  throw std::logic_error("ConstInitializer does not have a value");
}

}  // namespace initializer