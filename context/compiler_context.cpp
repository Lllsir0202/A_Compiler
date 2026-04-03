#include "compiler_context.h"

namespace context {
/**
 * @brief Report an error with source range and message.
 * It will alse throw a runtime error to indicate that an error has occurred, and the compilation process should be
 * terminated.
 */
auto context::CompilerContext::ReportError(frontend::SourceRange source_range, const std::string &msg) -> void {
  // Here we can format the error message and output it to the output stream
  (*out_) << "Error at " << source_range.ToString() << ": " << msg << '\n';
  throw std::runtime_error("Compilation error occurred.");
}

/**
 * @brief Report an error with source range and message.
 * It will alse throw a runtime error to indicate that an error has occurred, and the compilation process should be
 * terminated.
 */
auto context::CompilerContext::ReportError(frontend::ast::ASTNode *node, const std::string &msg) -> void {
  // We can get the source range from the AST node and call the other ReportError function
  (*out_) << "Error at Line " << node->GetLine() << ", Column " << node->GetColumn() << ": " << msg << '\n';
  throw std::runtime_error("Compilation error occurred.");
}

/**
 * @brief Report an error without source range information
 * @note if you want to report an error with source range information, please use the other two ReportError functions.
 */
auto context::CompilerContext::ReportError(const std::string &msg) -> void {
  (*out_) << "Error: " << msg << '\n';
  throw std::runtime_error("Compilation error occurred.");
}

/**
 * @brief Report a warning with source range and message.
 * @note refer to clang gcc implement, if set warning as error, we can also throw a runtime error.
 */
auto context::CompilerContext::ReportWarning(frontend::ast::ASTNode *node, const std::string &msg) -> void {
  if (warning_as_error_) {
    (*out_) << "Warning treated as error\n\n";
    ReportError(node, msg);
  } else {
    printf("\033[33mWarning at Line %d, Column %d: %s\033[0m\n", node->GetLine(), node->GetColumn(), msg.c_str());
  }
}

/**
 * @brief Report a warning without source range information
 */
auto context::CompilerContext::ReportWarning(const std::string &msg) -> void {
  if (warning_as_error_) {
    (*out_) << "Warning treated as error\n\n";
    ReportError(msg);
  } else {
    printf("\033[33mWarning: %s\033[0m\n", msg.c_str());
  }
}

auto context::CompilerContext::InitializeLibFunc() -> void {
  auto int_type = type_context_->GetIntegerType();
  auto float_type = type_context_->GetFloatType();
  auto void_type = type_context_->GetVoidType();
  auto int_ptr_type = type_context_->GetPtrType(int_type);
  auto float_ptr_type = type_context_->GetPtrType(float_type);

  // int getint();
  symboltable_context_->AddFunc("getint", int_type);

  // int getch();
  symboltable_context_->AddFunc("getch", int_type);

  // float getfloat();
  symboltable_context_->AddFunc("getfloat", float_type);

  // int getarray(int[]);
  symboltable_context_->AddFunc("getarray", int_type, {int_ptr_type});

  // int getfarray(float[]);
  symboltable_context_->AddFunc("getfarray", int_type, {float_ptr_type});

  // void putint(int);
  symboltable_context_->AddFunc("putint", void_type, {int_type});

  // void putch(int);
  symboltable_context_->AddFunc("putch", void_type, {int_type});

  // void putfloat(float);
  symboltable_context_->AddFunc("putfloat", void_type, {float_type});

  // void putarray(int[], int);
  symboltable_context_->AddFunc("putarray", void_type, {int_ptr_type, int_type});

  // void putfarray(float[], int);
  symboltable_context_->AddFunc("putfarray", void_type, {float_ptr_type, int_type});

  // void putf(char[], int);
  // Not deal with now

  // void _start_time_();
  symboltable_context_->AddFunc("start_time", void_type, {});

  // void _end_time_();
  symboltable_context_->AddFunc("end_time", void_type, {});
}

}  // namespace context