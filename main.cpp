#include <iostream>
#include <unordered_set>
// #include "frontend/utils/token.h"

#define DEBUG
static std::string file_name;
static std::string output_name;
static std::string stage = "llvm";
static int optimization_level = 0;

static const std::unordered_set<std::string> STAGES = {"lexer", "parser", "ast", "llvm", "S", "s"};

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
          std::cerr << "Invalid optimization option: " << arg << std::endl;
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
      } else {
        // invalid option
        std::cerr << "Invalid option: " << arg << std::endl;
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
    std::cerr << "Usage: " << argv[0]
              << " <input_file> -o <output_file> [-<stage>{lexer, parser, ast, llvm, S/s}] [-O<level>{0,1,2}]"
              << std::endl;
    return 1;
  }
  // Parser successful
#ifdef DEBUG
  std::cout << "Input File: " << file_name << std::endl;
  std::cout << "Output File: " << output_name << std::endl;
  std::cout << "Stage: " << stage << std::endl;
  std::cout << "Optimization Level: " << optimization_level << std::endl;
#endif

  // Following code would implement the actual compilation stages
  // We begin with tokenization
  if(stage == "lexer") {
    
  }

  std::cout << "Hello, World!" << std::endl;
  return 0;
}