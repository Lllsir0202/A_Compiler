#ifndef MIDDLEEND_BUILDER_MLIR_MLIRBUILDER_H
#define MIDDLEEND_BUILDER_MLIR_MLIRBUILDER_H
#include <memory>
#include "builder/irvisitor.h"
#include "context/compiler_context.h"
#include "middleend/builder/irbuilder.h"
#include "middleend/builder/mlir/mlirvisitor.h"

namespace middleend {
class MLIRBuilder : public IRBuilder {
 public:
  explicit MLIRBuilder(context::CompilerContext *compiler_context)
      : IRBuilder(std::unique_ptr<IRVisitor>(new MLIRVisitor(compiler_context)), compiler_context) {}
  ~MLIRBuilder() override = default;

 private:
  auto GetMLIRContext() -> context::MLIRContext * { return compiler_context_->GetIRContext()->GetMLIRContext(); }
};

}  // namespace middleend

#endif