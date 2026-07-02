#include <fstream>
#include <memory>
#include <unordered_set>
#include "backend/mir/lower/lower.h"
#include "context/compiler_context.h"
#include "context/pass_context.h"
#include "frontend/ast/visitor/semantic/sem_analysis.h"
#include "frontend/frontend.h"
#include "middleend/builder/mlir/mlirbuilder.h"
#include "middleend/llvm/lower/lower.h"
#include "middleend/llvm/verifer/verifer.h"
#include "middleend/mlir/lower/lower.h"
#include "middleend/mlir/verifer/verifer.h"
#include "middleend/pass/analysis/alias/alias.h"
#include "middleend/pass/analysis/cfg/mlircfg.h"
#include "middleend/pass/analysis/domtree/domtree.h"
#include "middleend/pass/analysis/loopinfo/loopinfo.h"
#include "middleend/pass/analysis/memoryssa/memoryssa.h"
#include "middleend/pass/transform/adce/adce.h"
#include "middleend/pass/transform/dse/dse.h"
#include "middleend/pass/transform/functioninline/functioninline.h"
#include "middleend/pass/transform/gvn/gvn.h"
#include "middleend/pass/transform/licm/licm.h"
#include "middleend/pass/transform/llvmtrivialblockmerge/llvmtrivialblockmerge.h"
#include "middleend/pass/transform/loopsimplify/loopsimplify.h"
#include "middleend/pass/transform/mlirdce/mlirdce.h"
#include "middleend/pass/transform/mlirfunctioninline/mlirfunctioninline.h"
#include "middleend/pass/transform/mlirreturncanonicalize/mlirreturncanonicalize.h"
#include "middleend/pass/transform/mlirtailrecursion/mlirtailrecursion.h"
#include "middleend/pass/transform/sccp/sccp.h"
#include "middleend/pass/transform/simplify/simplify.h"
#include "middleend/pass/transform/trivialblockmerge/trivialblockmerge.h"
#include "middleend/pass/transform/unify/unify.h"
#include "stages/ast/ast_stage.h"
#include "stages/lexer/lexer_stage.h"
#include "stages/llvm/llvm_stage.h"
#include "stages/mir/mir_stage.h"
#include "stages/mlir/mlir_stage.h"
#include "stages/parser/parser_stage.h"
#include "stages/stage.h"

#define DEBUG
#ifdef DEBUG
#include <iostream>
#endif
static std::string file_name;
static std::string output_name;
static std::string stage = "llvm";
static int optimization_level = 0;
// Mark whether regard warnings as errors, default to false
static bool warnings_as_errors = false;

static const std::unordered_set<std::string> STAGES = {"lexer", "parser", "ast", "mlir", "baseir",
                                                       "llvm",  "mir",    "S",   "s"};

auto static inline ParseArgs(int argc, char *argv[]) -> bool {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg[0] == '-') {
      // indicate that this arg is among {-o , -stage, -O}
      if (arg[1] == 'O') {
        if (arg.length() == 2) {
          // -O
          optimization_level = 0;
        } else if (arg.length() == 3) {
          // -O0, -O1
          optimization_level = arg[2] - '0';
        } else {
          // invalid -O option
          std::cerr << "Invalid optimization option: " << arg << '\n';
          return false;
        }
      } else if (arg[1] == 'o') {
        // -o output_name
        if (i + 1 < argc) {
          output_name = argv[++i];
        } else {
          // missing output name
          return false;
        }
      } else if (static_cast<bool>(STAGES.count(arg.substr(1)))) {
        // -stage
        stage = arg.substr(1);  // remove leading '-'
      } else if (arg == "-Werror" || arg == "--Werror") {
        warnings_as_errors = true;
      } else {
        // invalid option
        std::cerr << "Invalid option: " << arg << '\n';
        return false;
      }
    } else {
      // input file
      file_name = arg;
    }
  }
  // Ensure that both input file and output name are provided
  return !(file_name.empty() || output_name.empty());
}

auto main(int argc, char *argv[]) -> int {
  // Now we use different args to control different Stages
  if (argc <= 1 || !ParseArgs(argc, argv)) {
    std::cerr
        << "Usage: " << argv[0]
        << " <input_file> -o <output_file> [-<stage>{lexer, parser, ast, mlir, baseir, llvm, S/s}] [-O<level>{0,1,2}] "
           "[--Werror|-Werror]"
        << '\n';
    return 1;
  }
  // Parser successful
#ifdef DEBUG
  std::cout << "Input File: " << file_name << '\n';
  std::cout << "Output File: " << output_name << '\n';
  std::cout << "Stage: " << stage << '\n';
  std::cout << "Optimization Level: " << optimization_level << '\n';
#endif

  // Apply code to read and write files
  std::ifstream input(file_name);
  if (!input.is_open()) {
    std::cerr << "Error: Unable to open input file " << file_name << '\n';
    return 1;
  }

  std::ofstream output(output_name);
  if (!output.is_open()) {
    std::cerr << "Error: Unable to open/create output file " << output_name << '\n';
    return 1;
  }

// Following code would implement the actual compilation stages
// We begin with tokenization
#ifdef TOKENDEBUG
  auto &token_factory = frontend::TokenFactory::GetInstance();
  std::string lexeme = "const";
  auto range = frontend::SourceRange::GetRange(0, static_cast<uint32_t>(-1), 1, 0, 1, 6);
  auto token = token_factory.CreateToken(frontend::TokenType::CONST, lexeme, range);
  token.Print(std::cout);
#endif

  // Initialize all members
  static context::CompilerContext compiler_context(&input, &output, warnings_as_errors);
  std::unique_ptr<stages::Stage> stage_ptr;
  std::unique_ptr<frontend::Lexer> lexer = std::make_unique<frontend::FlexLexerImpl>(&input);

  // From parser stage we would build AST, so we should pass compiler context to each stage
// #define LEXERDEBUG
#ifdef LEXERDEBUG
  {
    auto token = lexer->GetNextToken();
    while (token.GetTokenType() != frontend::TokenType::END) {
      if (token.GetTokenType() != frontend::TokenType::UNKNOWN) {
        token.Print(std::cout);
      }
      token = lexer->GetNextToken();
    }
  }
#endif

  if (stage == "lexer") {
    stage_ptr = std::make_unique<stages::LexerStage>(std::move(lexer), output);
    stage_ptr->Run();
    return 0;
  }

#ifdef OPCODEDEBUG
  {
    using frontend::ast::OpCode;
    using frontend::ast::OpCodeToString;
    auto op = OpCode::PLUS;
    std::cout << "OpCode: " << OpCodeToString(op) << '\n';
  }
#endif

// #define CONVERTDEBUG
#ifdef CONVERTDEBUG
  {
    using frontend::StringToFloat;
    using frontend::StringToInt;
    std::vector<std::string> test_ints = {"123", "0", "-456", "0777", "0x1A3F", "111111111111"};
    std::vector<std::string> test_floats = {"3.14",     "0.0",  "-2.71",     "1e10",
                                            "6.022e23", "1e-2", "0x1.6FP03", "0x.6AFP-2"};
    for (const auto &str : test_ints) {
      int value = StringToInt(str);
      std::cout << "String: " << str << " -> Int: " << value << '\n';
    }
    for (const auto &str : test_floats) {
      float value = StringToFloat(str);
      std::cout << "String: " << str << " -> Float: " << value << '\n';
    }
  }
#endif

  // Grammar analysis and AST construction
  // CANNOT define parser before lexer's stage output.
  auto parser = std::make_unique<frontend::BisonParserImpl>(std::move(lexer));
  int ret = parser->Parse(compiler_context);

  if (ret != 0) {
    std::cerr << "Error: Parsing failed with error code " << ret << '\n';
    return ret;
  }

  if (stage == "parser" && ret == 0) {
    auto astprinter = std::make_unique<frontend::ast::ASTPrinter>(&output);
    stage_ptr = std::make_unique<stages::ParserStage>(&compiler_context, std::move(astprinter));
    stage_ptr->Run();
    return ret;
  }

  auto semantic_analyser = std::make_unique<frontend::ast::SemanticAnalyser>(compiler_context);
  ret = semantic_analyser->Analyse();

  if (ret != 0) {
    std::cerr << "Error: Semantic analysis failed with error code " << ret << '\n';
    return ret;
  }

  if (stage == "ast" && ret == 0) {
    auto astprinter = std::make_unique<frontend::ast::ASTPrinter>(&output);
    stage_ptr = std::make_unique<stages::ASTStage>(&compiler_context, std::move(astprinter));
    stage_ptr->Run();
    return ret;
  }

  auto mlir_builder = std::make_unique<middleend::MLIRBuilder>(compiler_context);
  ret = mlir_builder->Build();

  if (ret != 0) {
    std::cerr << "Error: MLIR building failed with error code " << ret << '\n';
    return ret;
  }

  // Verify the MLIR module
  auto module_op = compiler_context.GetIRContext()->GetMLIRContext()->GetModuleOp();
  auto mlir_verifier = std::make_unique<middleend::mlir::verifier::MLIRVerifier>(module_op, &compiler_context);
  if (!mlir_verifier->Verify()) {
    return 1;
  }

  if (stage == "mlir" && ret == 0) {
    stage_ptr = std::make_unique<stages::MLIRStage>(&compiler_context);
    stage_ptr->Run();
    return ret;
  }

  if (optimization_level == 1) {
    auto mlirpass_context = new context::PassContext(&compiler_context, middleend::pass::PassStage::MLIR);
    compiler_context.SetPassContext(mlirpass_context);
    auto mlir_return_canonicalize_pass =
        context::PassContext::CreatePass<middleend::pass::transform::MLIRReturnCanonicalizePass>(
            &compiler_context, middleend::pass::PassStage::MLIR);
    mlirpass_context->AddPass(mlir_return_canonicalize_pass);
    auto mlir_tail_recursion_eliminate_pass =
        context::PassContext::CreatePass<middleend::pass::transform::MLIRTailRecursionEliminatePass>(
            &compiler_context, middleend::pass::PassStage::MLIR);
    mlirpass_context->AddPass(mlir_tail_recursion_eliminate_pass);
    auto mlir_function_inline_pass =
        context::PassContext::CreatePass<middleend::pass::transform::MLIRFunctionInlinePass>(
            &compiler_context, middleend::pass::PassStage::MLIR);
    mlirpass_context->AddPass(mlir_function_inline_pass);
    auto mlir_dead_function_eliminate_pass =
        context::PassContext::CreatePass<middleend::pass::transform::MLIRDeadFunctionEliminatePass>(
            &compiler_context, middleend::pass::PassStage::MLIR);
    mlirpass_context->AddPass(mlir_dead_function_eliminate_pass);
    ret = mlirpass_context->RunPasses();
    if (ret != 0) {
      return 1;
    }
    if (!mlir_verifier->Verify()) {
      return 1;
    }
  }

  auto mlir_lower = std::make_unique<middleend::mlir::lower::MLIRLower>(&compiler_context);
  ret = mlir_lower->Lower();
  if (ret != 0) {
    return ret;
  }

  auto lower_verifier = std::make_unique<middleend::mlir::verifier::MLIRVerifier>(
      compiler_context.GetIRContext()->GetMLIRContext()->GetModuleOp(), &compiler_context);

  if (!lower_verifier->Verify()) {
    return -1;
  }

  auto baseirpass_context = new context::PassContext(&compiler_context, middleend::pass::PassStage::BASEIR);
  compiler_context.SetPassContext(baseirpass_context);
  auto baseir_cfg_builder = new middleend::pass::analysis::MLIRCFGBuilder(&compiler_context);
  auto cfg_pass = context::PassContext::CreatePass<middleend::pass::analysis::CFGPass>(
      &compiler_context, baseir_cfg_builder, middleend::pass::PassStage::BASEIR);
  baseirpass_context->AddPass(cfg_pass);
  auto simplify_pass = context::PassContext::CreatePass<middleend::pass::transform::SimplifyPass>(
      &compiler_context, middleend::pass::PassStage::BASEIR);
  baseirpass_context->AddPass(simplify_pass);
  auto unify_pass = context::PassContext::CreatePass<middleend::pass::transform::UnifyReturnPass>(
      &compiler_context, middleend::pass::PassStage::BASEIR);
  baseirpass_context->AddPass(unify_pass);

  if (optimization_level == 1) {
    // auto function_inline_pass = context::PassContext::CreatePass<middleend::pass::transform::FunctionInlinePass>(
    // &compiler_context, middleend::pass::PassStage::BASEIR);
    // baseirpass_context->AddPass(function_inline_pass);
    auto domtree_pass = context::PassContext::CreatePass<middleend::pass::analysis::DomTreePass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(domtree_pass);
    auto adce_pass = context::PassContext::CreatePass<middleend::pass::transform::ADCEPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(adce_pass);

    auto sccp_pass = context::PassContext::CreatePass<middleend::pass::transform::SCCPPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(sccp_pass);
    auto post_sccp_simplify_pass = context::PassContext::CreatePass<middleend::pass::transform::PostSCCPSimplifyPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(post_sccp_simplify_pass);
    auto post_sccp_adce_pass = context::PassContext::CreatePass<middleend::pass::transform::PostSCCPADCEPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(post_sccp_adce_pass);

#ifndef DISABLE_NEW_OPTS
    auto alias_analysis_pass = context::PassContext::CreatePass<middleend::pass::analysis::AliasAnalysisPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(alias_analysis_pass);
    auto memory_ssa_pass = context::PassContext::CreatePass<middleend::pass::analysis::MemorySSAPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(memory_ssa_pass);
    auto gvn_pass = context::PassContext::CreatePass<middleend::pass::transform::GVNPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(gvn_pass);
    auto dse_pass = context::PassContext::CreatePass<middleend::pass::transform::DSEPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(dse_pass);
    auto post_dse_adce_pass = context::PassContext::CreatePass<middleend::pass::transform::PostDSEADCEPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(post_dse_adce_pass);
    auto loop_info_pass = context::PassContext::CreatePass<middleend::pass::analysis::LoopInfoPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(loop_info_pass);
    auto loop_simplify_pass = context::PassContext::CreatePass<middleend::pass::transform::LoopSimplifyPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(loop_simplify_pass);
    auto licm_pass = context::PassContext::CreatePass<middleend::pass::transform::LICMPass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(licm_pass);
#endif

    auto trivial_block_merge_pass = context::PassContext::CreatePass<middleend::pass::transform::TrivialBlockMergePass>(
        &compiler_context, middleend::pass::PassStage::BASEIR);
    baseirpass_context->AddPass(trivial_block_merge_pass);
  }

  ret = baseirpass_context->RunPasses();

  if (ret != 0) {
    return 1;
  }

  if (stage == "baseir" && ret == 0) {
    stage_ptr = std::make_unique<stages::MLIRStage>(&compiler_context);
    stage_ptr->Run();
    return ret;
  }

  auto llvm_lower = std::make_unique<middleend::llvm::lower::LLVMLower>(&compiler_context);
  ret = llvm_lower->Lower();

  auto llvm_verifier = std::make_unique<middleend::llvm::verifer::LLVMVerifer>(
      compiler_context.GetIRContext()->GetLLVMIRContext()->GetModule(), &compiler_context);

  if (!llvm_verifier->Verify()) {
    return -1;
  }

  auto llvmirpass_context = new context::PassContext(&compiler_context, middleend::pass::PassStage::LLVMIR);
  compiler_context.SetPassContext(llvmirpass_context);
  auto llvm_trivial_block_merge_pass =
      context::PassContext::CreatePass<middleend::pass::transform::LLVMIRTrivialBlockMergePass>(
          &compiler_context, middleend::pass::PassStage::LLVMIR);
  llvmirpass_context->AddPass(llvm_trivial_block_merge_pass);
  ret = llvmirpass_context->RunPasses();
  if (ret != 0) {
    return 1;
  }

  if (!llvm_verifier->Verify()) {
    return -1;
  }

  if (stage == "llvm" && ret == 0) {
    stage_ptr = std::make_unique<stages::LLVMStage>(&compiler_context);
    stage_ptr->Run();
    return ret;
  }

  auto mir_lower = std::make_unique<backend::lower::MIRLower>(&compiler_context);
  ret = mir_lower->Lower();

  if (ret != 0) {
    return 1;
  }

  if (stage == "mir" && ret == 0) {
    stage_ptr = std::make_unique<stages::MIRStage>(&compiler_context);
    stage_ptr->Run();
    return ret;
  }

  return ret;
  // std::cout << "Hello, World!" << '\n';
  // return 0;
}
