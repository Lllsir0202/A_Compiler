#ifndef UTILS_FLATTEN_UTILS_H
#define UTILS_FLATTEN_UTILS_H

#include "ast/ast_decl.h"

namespace utils::flatten {

/**
 * @brief Object class: represent the current object which is being initialized
 * @param current_initialized_object: the current object which is being initialized, used for calculating the offset
 * of the initializer list
 */
class Object {
 public:
  explicit Object(std::vector<int> current_initialized_object)
      : current_initialized_object_(std::move(current_initialized_object)){};

  auto IsScalar() const -> bool { return current_initialized_object_.empty(); }

  std::vector<int> current_initialized_object_;
};

/**
 * @brief InitCursor class: represent the current position in the initializer
 */
class InitCursor {
 public:
  explicit InitCursor(std::vector<frontend::ast::Initializer *> init_list) : init_list_(std::move(init_list)) {}

  auto Next() -> frontend::ast::Initializer * {
    if (pos_ < init_list_.size()) {
      return init_list_[pos_++];
    }
    return nullptr;
  }

  auto Peek() -> frontend::ast::Initializer * {
    if (pos_ < init_list_.size()) {
      return init_list_[pos_];
    }
    return nullptr;
  }

  auto HasMore() -> bool { return !init_list_.empty(); }

  size_t pos_{};
  std::vector<frontend::ast::Initializer *> init_list_;
};

/**
 * @brief Address, represent the address of the position will be filled
 * @example int a[10][10] , we need to fill [1][2],
 * then the address will be [1, 2]
 */
class Address {
 public:
  Address(const Address &other) = default;
  explicit Address() = default;
  explicit Address(std::vector<int> dims) : dims_(std::move(dims)) {}

  auto operator[](size_t index) -> Address {
    Address new_address(*this);
    new_address.address_.emplace_back(index);
    return new_address;
  }
  auto Linear() -> int {
    /**
     * @example
     * int a[10][5], we need to fill [1][2]
     * the address will be [1,2],
     * the dims will be [10, 5], dim[0] = 10, dim[1] = 5
     * the linear address will be: 1 * 5 + 2 = 7
     * := address[0] * dim[1] + address[1]
     */
    int linear_address = 0;
    int multiplier = 1;
    for (int i = static_cast<int>(address_.size()) - 1; i >= 0; i--) {
      linear_address += address_[i] * multiplier;
      multiplier *= dims_[i];
    }
    return linear_address;
  }

  std::vector<int> address_;
  std::vector<int> dims_;
};
}  // namespace utils::flatten

#endif