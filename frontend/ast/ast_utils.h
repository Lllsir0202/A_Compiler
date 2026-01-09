#ifndef AST_UTILS_H
#define AST_UTILS_H

#include <cstdint>

namespace frontend::ast {
enum class OpCode : uint8_t {
#define OP_CODE(name) name,
#include "op_code.def"
#undef OP_CODE
};
}  // namespace frontend::ast

#endif