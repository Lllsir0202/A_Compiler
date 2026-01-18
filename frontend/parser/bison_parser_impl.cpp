#include "bison_parser_impl.h"
#include "bison.h"
#include "parser_utils.h"

namespace frontend {

auto BisonParserImpl::GetContext() -> context::CompilerContext & {
  assert(compiler_context_ != nullptr && "Compiler context is not set");
  return *compiler_context_;
}

auto BisonParserImpl::Parse(context::CompilerContext &context) -> int {
  // Set the compiler context member
  compiler_context_ = &context;

  try {
    // Create bison parser instance
    yy::parser parser(*this);
    // Invoke the parse method
    parser.parse();
  } catch (const std::runtime_error &e) {
    // Handle parsing errors if necessary
    compiler_context_ = nullptr;
    return 1;
  }

  // Clear the context after parsing
  compiler_context_ = nullptr;
  return 0;
}
/**
 * @brief We briefly introduce how symbol_type looks like in bison generated parser
 * @code
 *
 * struct symbol_type {
 *  token_kind_type kind;
 *  semantic_type   value;
 *  location_type   location;
 *};
 * @endcode
 * @note Here we redefine location_type as SourceRange in our bison parser
 * so that we can keep track of source code locations easily.
 */
}  // namespace frontend