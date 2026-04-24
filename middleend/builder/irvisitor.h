#ifndef MIDDLEEND_BUILDER_IRVISITOR_H
#define MIDDLEEND_BUILDER_IRVISITOR_H
#include "context/compiler_context.h"
#include "frontend/ast/astvisitor.h"

namespace middleend {

class IRVisitor : public frontend::ast::ASTVisitor {
 public:
  ~IRVisitor() override = default;

 protected:
  explicit IRVisitor(context::CompilerContext *context) : compiler_context_(context) {}

  context::CompilerContext *compiler_context_;
};

}  // namespace middleend
#endif