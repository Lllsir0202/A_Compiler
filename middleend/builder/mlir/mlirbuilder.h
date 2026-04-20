#ifndef MIDDLEEND_BUILDER_MLIR_MLIRBUILDER_H
#define MIDDLEEND_BUILDER_MLIR_MLIRBUILDER_H
#include "context/compiler_context.h"
#include "middleend/builder/irbuilder.h"
#include "middleend/builder/mlir/mlirvisitor.h"

namespace middleend {
class MLIRBuilder : public IRBuilder {
 public:
  explicit MLIRBuilder(MLIRVisitor *visitor, context::CompilerContext *compiler_context)
      : IRBuilder(visitor, compiler_context) {}
  ~MLIRBuilder() override = default;
};

}  // namespace middleend

#endif