#ifndef MIDDLEEND_BUILDER_IRBUILDER_H
#define MIDDLEEND_BUILDER_IRBUILDER_H

#include <memory>
#include <stdexcept>
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

  auto Build() -> int {
    try {
      BuildModule(visitor_.get());

    } catch (std::logic_error &e) {
      compiler_context_->Print(e.what());
      return 1;
    } catch (std::runtime_error &e) {
      compiler_context_->Print(e.what());
      return 2;
    } catch (...) {
      compiler_context_->Print("Unknown error occurred while building IR.");
      return 3;
    }
    compiler_context_ = nullptr;
    return 0;
  }

  auto BuildModule(IRVisitor *visitor) -> void {
    auto root = compiler_context_->GetAstContext()->GetRootNode();
    root->Accept(visitor);
  }

 protected:
  std::unique_ptr<IRVisitor> visitor_;
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend

#endif