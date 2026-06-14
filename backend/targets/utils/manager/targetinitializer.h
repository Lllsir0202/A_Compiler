#ifndef BACKEND_TARGETS_UTILS_MANAGER_TARGETINITIALIZER_H
#define BACKEND_TARGETS_UTILS_MANAGER_TARGETINITIALIZER_H

#include <string>
#include <unordered_map>
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
  virtual ~TargetInitializer() = default;
  auto Initialize(context::BackendContext *context, int optimization_level) -> void {
    InitPhysicalRegPool(context);
    InitTargetPrinter();

    // This function must be called after the physical register pool is initialized.
    InitRAManager(context, optimization_level);
  }

 private:
  // Need to initialize the physical register pool, called in the constructor of backend context.
  virtual auto InitPhysicalRegPool(context::BackendContext *context) -> void = 0;
  virtual auto InitTargetPrinter() -> void = 0;
  // Initialize the physical register for register allocation.
  virtual auto InitRAManager(context::BackendContext *context, int optimization_level) -> void = 0;
};

class TargetInitializerRegistry {
 public:
  ~TargetInitializerRegistry() {
    for (auto &entry : registry_) {
      delete entry.second;
    }
    registry_.clear();
  }

  static auto GetInstance() -> TargetInitializerRegistry & {
    static TargetInitializerRegistry instance;
    return instance;
  }

  void RegisterTargetInitializer(const std::string &target_arch, TargetInitializer *initializer) {
    registry_[target_arch] = initializer;
  }

  auto GetTargetInitializer(const std::string &target_arch) -> TargetInitializer * {
    auto it = registry_.find(target_arch);
    if (it != registry_.end()) {
      return it->second;
    }
    return nullptr;
  }

 private:
  std::unordered_map<std::string, TargetInitializer *> registry_;
};

}  // namespace backend::targetir

#endif