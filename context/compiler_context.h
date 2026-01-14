#ifndef CONTEXT_COMPILER_CONTEXT_H
#define CONTEXT_COMPILER_CONTEXT_H

#include "context/ast_context.h"
#include "context/type_context.h"
namespace context {

/**
 * @brief Top context class for the compiler
 * This class will hold global information and states needed during the compilation process.
 */
class CompilerContext {
  // First we control types pointer here
  [[maybe_unused]] TypeContext type_context_;
  // Then we need to control ast context here
  [[maybe_unused]] AstContext ast_context_;
};

}  // namespace context

#endif