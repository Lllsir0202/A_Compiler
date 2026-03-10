#include "compiler_context.h"

namespace context {
auto context::CompilerContext::ReportError(frontend::SourceRange source_range, const std::string &msg) -> void {
  // Here we can format the error message and output it to the output stream
  (*out_) << "Error at " << source_range.ToString() << ": " << msg << '\n';
  throw std::runtime_error("Compilation error occurred.");
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
  symboltable_context_->AddFunc("_start_time_", void_type, {});

  // void _end_time_();
  symboltable_context_->AddFunc("_end_time_", void_type, {});
}

}  // namespace context