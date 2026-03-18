#ifndef UTILS_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_INITIALIZER_H

#include <memory>
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

}  // namespace initializer

#endif