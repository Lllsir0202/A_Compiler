#ifndef MIDDLEEND_BUILDER_IRBUILDER_H
#define MIDDLEEND_BUILDER_IRBUILDER_H

#include <memory>
#include "builder/irvisitor.h"
namespace middleend {
/**
 * @brief IRBuilder is the base class of the IR builder
 * the concrete builder should inherit from this class
 * and implement the BuildModule function

 * @note The IRBuilder will never be used directly,
 * however, it provides the common interface for the concrete builder.
 * But we will change the `IRVisitor` to the concrete visitor in the
 * concrete builder, so that we can use the concrete visitor
 * to traverse the AST and generate the IR.
 */
class IRBuilder {
 protected:
  explicit IRBuilder(std::unique_ptr<IRVisitor> visitor, context::CompilerContext *compiler_context)
      : visitor_(std::move(visitor)), compiler_context_(compiler_context) {}

 public:
  virtual ~IRBuilder() = default;

  auto Build() -> void { BuildModule(visitor_.get()); }

  auto BuildModule(IRVisitor *visitor) -> void {
    auto root = compiler_context_->GetAstContext()->GetRootNode();
    root->Accept(visitor);
  }

 private:
  std::unique_ptr<IRVisitor> visitor_;
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend

#endif