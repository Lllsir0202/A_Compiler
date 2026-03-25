#ifndef UTILS_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_INITIALIZER_H

#include <cstddef>
#include <memory>
#include <vector>
#include "utils/flatten/utils.h"

namespace initializer {

/** This class is the virtual class of the initializer
 * @brief All detailed initializers should inherit from this class
 */
class Initializer {
 public:
  Initializer() = default;
  virtual ~Initializer() = default;
  virtual auto IsScalar() const -> bool = 0;
  virtual auto IsConst() const -> bool = 0;
};

using InitializerPtr = std::unique_ptr<Initializer>;

/**
 * @brief This class represents a constant array initializer
 */
class ArrayInitializer : public Initializer {
 public:
  ArrayInitializer() = default;

  // disable copy
  ArrayInitializer(const ArrayInitializer &) = delete;
  auto operator=(const ArrayInitializer &) -> ArrayInitializer & = delete;

  // allow move
  ArrayInitializer(ArrayInitializer &&) noexcept = default;
  auto operator=(ArrayInitializer &&) noexcept -> ArrayInitializer & = default;

  ~ArrayInitializer() override = default;
  auto AddInitializer(InitializerPtr init, utils::flatten::Address address) -> void {
    size_t index = address.Linear();
    if (index != init_values_.size()) {
      throw std::logic_error("Initializer address is not in order");
    }
    init_values_.emplace_back(std::move(init));
  }
  auto GetInitializers() const -> const std::vector<InitializerPtr> & { return init_values_; }
  auto GetUpdateInitializers() -> std::vector<InitializerPtr> & { return init_values_; }
  auto IsScalar() const -> bool override { return false; }
  auto IsConst() const -> bool override { throw std::logic_error("ArrayInitializer does not have a constant value"); }

 private:
  std::vector<InitializerPtr> init_values_;
};

}  // namespace initializer

#endif