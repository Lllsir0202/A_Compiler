#ifndef UTILS_OPTION_OPTION_H
#define UTILS_OPTION_OPTION_H
#include <string>
#include <unordered_set>

namespace pipeline {
class Pipeline;
}

namespace option {
class Option {
 public:
  Option() = default;
  auto ParseArgs(int argc, char *argv[]) -> bool {
    for (int i = 1; i < argc; i++) {
      std::string arg = argv[i];
      if (arg[0] == '-') {
        // indicate that this arg is among {-o , -stage, -O}
        if (arg[1] == 'O') {
          if (arg.length() == 2) {
            // -O
            optimization_level_ = 0;
          } else if (arg.length() == 3) {
            // -O0, -O1
            optimization_level_ = arg[2] - '0';
          } else {
            // invalid -O option
            printf("\33[31mInvalid optimization option: %s\n\33[0m", arg.c_str());
            return false;
          }
        } else if (arg[1] == 'o') {
          // -o output_name
          if (i + 1 < argc) {
            output_name_ = argv[++i];
          } else {
            // missing output name
            return false;
          }
        } else if (static_cast<bool>(STAGES.count(arg.substr(1)))) {
          // -stage
          stage_ = arg.substr(1);  // remove leading '-'
        } else if (arg == "-Werror" || arg == "--Werror") {
          warnings_as_errors_ = true;
        } else {
          // invalid option
          printf("\33[31mInvalid option: %s\n\33[0m", arg.c_str());
          return false;
        }
      } else {
        // input file
        file_name_ = arg;
      }
    }
    // Ensure that both input file and output name are provided
    return !(file_name_.empty() || output_name_.empty());
  }

 protected:
  std::string file_name_;
  std::string output_name_;
  std::string stage_ = "llvm";
  int optimization_level_ = 0;
  // Mark whether regard warnings as errors, default to false
  bool warnings_as_errors_ = false;

  static const std::unordered_set<std::string> STAGES;
  friend class pipeline::Pipeline;
};

const std::unordered_set<std::string> Option::STAGES = {"lexer", "parser", "ast", "llvm", "S", "s"};

}  // namespace option

#endif