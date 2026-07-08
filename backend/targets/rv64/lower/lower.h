#ifndef BACKEND_TARGETS_RV64_LOWER_LOWER_H
#define BACKEND_TARGETS_RV64_LOWER_LOWER_H

#include "backend/targets/rv64/lower/lowerhelper.h"
#include "context/compiler_context.h"

namespace backend::targetir::rv64 {
class RV64Lower {
 public:
  explicit RV64Lower(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {
    lower_helper_ = new LowerHelper(compiler_context);
  }
  auto Lower() -> int;
  ~RV64Lower() { delete lower_helper_; }

 private:
  context::CompilerContext *compiler_context_;
  LowerHelper *lower_helper_;

  // Ignore the llvm-related library functions
  std::unordered_set<std::string> library_functions_ = {"putint",    "getint",    "putfloat",        "getfloat",
                                                        "putch",     "getch",     "putarray",        "getarray",
                                                        "putfarray", "getfarray", "_sysy_starttime", "_sysy_stoptime"};
  std::unordered_set<std::string> llvm_library_functions_ = {"llvm.memcpy.p0i8.p0i8.i64", "llvm.memset.p0i8.i64"};
};
}  // namespace backend::targetir::rv64

#endif