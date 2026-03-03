#include "compiler_context.h"

namespace context {
auto context::CompilerContext::ReportError(frontend::SourceRange source_range, const std::string &msg) -> void {
  // Here we can format the error message and output it to the output stream
  (*out_) << "Error at " << source_range.ToString() << ": " << msg << '\n';
  throw std::runtime_error("Compilation error occurred.");
}
}  // namespace context