#ifndef PIPELINE_PIPELINE_H
#define PIPELINE_PIPELINE_H

#include "compiler_context.h"
#include "option/option.h"
#include "stage.h"
namespace pipeline {

class Pipeline {
 public:
  explicit Pipeline(option::Option option, context::CompilerContext *compiler_context)
      : option_(std::move(option)), compiler_context_(compiler_context) {};

  auto Run() -> int;

 private:
  // option: arguments passed by command line
  option::Option option_;
  // context: store all the information during compilation
  [[maybe_unused]] context::CompilerContext *compiler_context_;
  // stage_ptr: pointer to the stage we aim to run
  std::unique_ptr<stages::Stage> stage_ptr_;
};

}  // namespace pipeline
#endif