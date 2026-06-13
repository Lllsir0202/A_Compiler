#ifndef BACKEND_TARGETS_UTILS_MANAGER_TARGETINITIALIZER_H
#define BACKEND_TARGETS_UTILS_MANAGER_TARGETINITIALIZER_H

namespace context {
class BackendContext;
}

namespace backend::targetir {
/**
 * @brief This class defines the target initializer, which will be registered as a sigleton in the backend
 * and pass to the BackendContext to initialize the target specific components.
 */
class TargetInitializer {
 public:
  ~TargetInitializer() = default;

 private:
  // Need to initialize the physical register pool, called in the constructor of backend context.
  virtual auto InitPhysicalRegPool() -> void = 0;

  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif