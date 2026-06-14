#ifndef CONTEXT_COMPILER_CONTEXT_H
#define CONTEXT_COMPILER_CONTEXT_H

#include <fstream>
#include "ast/ast_base.h"
#include "context/ast_context.h"
#include "context/backend_context.h"
#include "context/ir_context.h"
#include "context/mir_context.h"
#include "context/pass_context.h"
#include "context/symboltable_context.h"
#include "context/type_context.h"
#include "targets/aarch64/initializer/aarch64initializer.h"
#include "targets/rv64/initializer/rv64initializer.h"
#include "targets/utils/manager/targetinitializer.h"
namespace context {

/**
 * @brief Top context class for the compiler
 * This class will hold global information and states needed during the compilation process.
 */
class CompilerContext {
  // We can create TypeContext at the beginning since it doesn't depend on anything else
  // and will be used throughout the compilation process.
 public:
  CompilerContext(std::ifstream *in, std::ofstream *out, const std::string &target_arch = "rv64",
                  int optimization_level = 0, bool warnings_as_errors = false, bool output_region = false)
      : in_(in),
        out_(out),
        warning_as_error_(warnings_as_errors),
        output_region_(output_region),
        optimization_level_(optimization_level) {
    type_context_ = new TypeContext();
    ast_context_ = new AstContext();
    symboltable_context_ = new SymbolTableContext();
    InitializeLibFunc();
    ir_context_ = new IRContext(type_context_);
    mir_context_ = new MIRContext(type_context_);

    // Register the target initializer and create the backend context
    RegisterTargetInitializers();
    backend_context_ = new BackendContext(target_arch, optimization_level);
  }
  ~CompilerContext() {
    delete type_context_;
    delete ast_context_;
    delete symboltable_context_;
    delete ir_context_;
    delete pass_context_;
    delete mir_context_;
    delete backend_context_;
  }
  auto GetTypeContext() -> TypeContext * { return type_context_; }
  auto GetAstContext() -> AstContext * { return ast_context_; }
  auto GetSymbolTableContext() -> SymbolTableContext * { return symboltable_context_; }
  auto GetIRContext() -> IRContext * { return ir_context_; }
  auto GetMIRContext() -> MIRContext * { return mir_context_; }
  auto SetPassContext(PassContext *pass_context) -> void { pass_context_ = pass_context; }
  auto GetPassContext() -> PassContext * { return pass_context_; }
  auto ReportError(frontend::SourceRange source_range, const std::string &msg) -> void;
  auto ReportError(frontend::ast::ASTNode *node, const std::string &msg) -> void;
  auto ReportError(const std::string &msg) -> void;
  auto Print(const std::string &msg) -> void { (*out_) << msg; }
  auto ReportWarning(frontend::ast::ASTNode *node, const std::string &msg) -> void;
  auto ReportWarning(const std::string &msg) -> void;
  // Get the output stream
  auto GetOutputStream() -> std::ofstream * { return out_; }
  // Get the output region flag
  auto GetOutputRegionFlag() -> bool { return output_region_; }

 private:
  // First we control types pointer here
  TypeContext *type_context_;
  // Then we need to control ast context here
  AstContext *ast_context_;
  // We need to control symbol table context
  SymbolTableContext *symboltable_context_;

  // we will use the ircontext to control the resources of the IR
  IRContext *ir_context_;

  // Pass context, used in optimization
  PassContext *pass_context_;

  // Machine IR context
  MIRContext *mir_context_;

  // Backend context
  BackendContext *backend_context_;

  // Using this compiler context to control ALL resources in the compiler

  /**
   * @brief In the future, we will add input/output stream management here
   * and we will contruct lexer and parser using these streams
   */
  [[maybe_unused]] std::ifstream *in_;
  std::ofstream *out_;

  /**
   * @brief Register library functions in the Sysy
   */
  auto InitializeLibFunc() -> void;

  /**
   * @brief Warning as error flag
   */
  bool warning_as_error_;
  /**
   * @brief Output region flag
   */
  bool output_region_{};
  /**
   * @brief Optimization level
   */
  int optimization_level_;

  // Register the target initializers
  auto RegisterTargetInitializers() -> void;
};

}  // namespace context

#endif