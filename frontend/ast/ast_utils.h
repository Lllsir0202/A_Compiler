#ifndef AST_UTILS_H
#define AST_UTILS_H

#include <cstdint>
#include <string>

namespace frontend::ast {
// define OpCode enum class
enum class OpCode : uint8_t {
#define OP_CODE(name) name,
#include "op_code.def"
#undef OP_CODE
};

// Define Helper used to transform OpCode to string
[[maybe_unused]] inline auto OpCodeToString(OpCode type) -> std::string {
  switch (type) {
#define OP_CODE(name) \
  case OpCode::name:  \
    return #name;
#include "op_code.def"
#undef OP_CODE
    default:
      return "UNKNOWN_OPCODE";
  }
}
}  // namespace frontend::ast

#endif