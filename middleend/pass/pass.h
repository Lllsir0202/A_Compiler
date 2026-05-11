#ifndef MIDDLEEND_PASS_PASS_H
#define MIDDLEEND_PASS_PASS_H
#include <string>
namespace context {
class CompilerContext;
}
namespace middleend::pass {
// Base class for all passes
class Pass {
 public:
  virtual ~Pass() = default;
  virtual void Run() = 0;
  // Verify the pass, check if the pass can be run on the current stage,
  // also check if the pass's dependencies are satisfied, etc.
  virtual auto Verify() -> bool { return true; }
  virtual auto Name() const -> std::string = 0;

 protected:
  explicit Pass(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  context::CompilerContext *compiler_context_;
};

}  // namespace middleend::pass

#endif