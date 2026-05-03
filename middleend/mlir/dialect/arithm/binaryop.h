#ifndef MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_ARITHM_BINARYOP_H
#include "context/compiler_context.h"
#include "middleend/mlir/dialect/arithm/basic.h"
#include "middleend/mlir/utils/value/value.h"
namespace middleend::mlir::arithm {

// #include "middleend/mlir/dialect/arithm/binaryop.def"
/**
 * @brief Since most of the binary operations have the same structure,
 * we can use a macro to define these binary operations
 */
#define DEFINE_BINARY_OP(NAME)                                                                            \
  class NAME##Op : public ArithmOps {                                                                     \
   public:                                                                                                \
    ~NAME##Op() override = default;                                                                       \
    static auto Create(context::MLIRContext *context, mlir::Value *lhs, mlir::Value *rhs) -> NAME##Op * { \
      auto op = new NAME##Op();                                                                           \
      op->SetUp(context, lhs, rhs);                                                                       \
      return op;                                                                                          \
    }                                                                                                     \
    auto GetLHS() -> mlir::Value * { return operands_[0]; }                                               \
    auto GetRHS() -> mlir::Value * { return operands_[1]; }                                               \
    auto GetRes() -> mlir::Value * { return results_[0]; }                                                \
    auto Emit(std::ofstream *out) -> void override {                                                      \
      /* Implementation for emitting the operation */                                                     \
      PrintIndent(out);                                                                                   \
      results_[0]->Emit(out);                                                                             \
      *out << " = " << Name() << " ";                                                                     \
      operands_[0]->Emit(out);                                                                            \
      *out << " , ";                                                                                      \
      operands_[1]->Emit(out);                                                                            \
      *out << "\n";                                                                                       \
    }                                                                                                     \
                                                                                                          \
    auto Verify([[maybe_unused]] context::CompilerContext *context) -> bool override {                    \
      /* Implementation for verifying the operation */                                                    \
      if (operands_.size() != 2) {                                                                        \
        context->Print("BinaryOp: operand size is not 2\n");                                              \
        return false;                                                                                     \
      }                                                                                                   \
      if (results_.size() != 1) {                                                                         \
        context->Print("BinaryOp: result size is not 1\n");                                               \
        return false;                                                                                     \
      }                                                                                                   \
      if (results_[0]->GetType() == nullptr) {                                                            \
        context->Print("BinaryOp: result has null type\n");                                               \
        return false;                                                                                     \
      }                                                                                                   \
      if (operands_[0]->type_ != operands_[1]->type_) {                                                   \
        context->Print("BinaryOp: operand types do not match\n");                                         \
        return false;                                                                                     \
      }                                                                                                   \
      return true;                                                                                        \
    }                                                                                                     \
                                                                                                          \
   private:                                                                                               \
    NAME##Op() : ArithmOps(#NAME) {}                                                                      \
    auto SetUp(context::MLIRContext *context, mlir::Value *lhs, mlir::Value *rhs) -> void {               \
      /* NAME##Op has two operands and one result */                                                      \
      operands_.reserve(2);                                                                               \
      results_.reserve(1);                                                                                \
      operands_.emplace_back(lhs);                                                                        \
      operands_.emplace_back(rhs);                                                                        \
      auto res = context->CreateValue();                                                                  \
      results_.emplace_back(res);                                                                         \
      res->op_ = this;                                                                                    \
    }                                                                                                     \
  };

DEFINE_BINARY_OP(Add)
DEFINE_BINARY_OP(Sub)
DEFINE_BINARY_OP(Mul)
DEFINE_BINARY_OP(Div)
DEFINE_BINARY_OP(Mod)
DEFINE_BINARY_OP(Equal)
DEFINE_BINARY_OP(NotEqual)
DEFINE_BINARY_OP(Less)
DEFINE_BINARY_OP(LessEqual)
DEFINE_BINARY_OP(Greater)
DEFINE_BINARY_OP(GreaterEqual)

/**
 * @note Since we need to consider the short-circuiting behavior of the logical operations,
 * we should use ifOp to implement the logical operations,
 * instead of directly using the logical operation
 */
// DEFINE_BINARY_OP(And)
// DEFINE_BINARY_OP(Or)
}  // namespace middleend::mlir::arithm
#endif