#ifndef MIDDLEEND_MLIR_UTILS_ADAPTER_ADAPTER_H
#define MIDDLEEND_MLIR_UTILS_ADAPTER_ADAPTER_H

namespace middleend::mlir {
class Operation;

class Adapter {
 public:
  virtual auto Adapt(Operation *op) -> void = 0;
};

// Implement a manager for adapters
class AdapterManager {};
}  // namespace middleend::mlir

#endif