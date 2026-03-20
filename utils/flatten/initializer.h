#ifndef UTILS_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_INITIALIZER_H

#include <memory>
#include <vector>
namespace initializer {

/** This class is the virtual class of the initializer
 * @brief All detailed initializers should inherit from this class
 */
class Initializer {
 public:
  Initializer() = default;
  virtual ~Initializer() = default;
  virtual auto IsScalar() const -> bool = 0;
};

using InitializerPtr = std::unique_ptr<Initializer>;

/**
 * @brief This class represents a constant array initializer
 */
class ArrayInitializer : public Initializer {
 public:
  auto AddInitializer(InitializerPtr init) -> void { init_values_.push_back(std::move(init)); }
  auto GetInitializers() const -> const std::vector<InitializerPtr> & { return init_values_; }
  auto IsScalar() const -> bool override { return false; }

 private:
  std::vector<InitializerPtr> init_values_;
};

}  // namespace initializer

#endif