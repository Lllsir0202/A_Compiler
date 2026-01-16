#include "bison_parser_impl.h"
#include "bison.h"

namespace frontend {

auto BisonParserImpl::GetContext() -> context::CompilerContext & {
  assert(compiler_context_ != nullptr && "Compiler context is not set");
  return *compiler_context_;
}

auto BisonParserImpl::Parse(context::CompilerContext &context) -> void {
  // Set the compiler context member
  compiler_context_ = &context;

  // Create bison parser instance
  yy::parser parser(*this);
  // Invoke the parse method
  parser.parse();

  // Clear the context after parsing
  compiler_context_ = nullptr;
}

}  // namespace frontend