#ifndef MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#include "middleend/mlir/dialect/base/basic.h"
#include "mlir_context.h"
namespace middleend::mlir::base {

class AddOp : public BaseOps {
 public:
  ~AddOp() override = default;
  static auto Create(context::MLIRContext *context, middleend::mlir::Value *lhs,
                     middleend::mlir::Value *rhs) -> AddOp * {
    auto op = new AddOp();
    op->SetUp(context, lhs, rhs);
    return op;
  }

  auto Emit(std::ofstream *out) -> void override {
    *out << "  " << Name() << " " << operands_[0]->num_ << ", " << operands_[1]->num_ << "\n";
  }

  auto GetLhs() -> middleend::mlir::Value * { return operands_[0]; }
  auto GetRhs() -> middleend::mlir::Value * { return operands_[1]; }
  auto GetRes() -> middleend::mlir::Value * { return results_[0]; }

 private:
  AddOp() : BaseOps("add") {}
  auto SetUp([[maybe_unused]] context::MLIRContext *context, middleend::mlir::Value *lhs,
             middleend::mlir::Value *rhs) -> void {
    operands_.reserve(2);
    operands_.emplace_back(lhs);
    operands_.emplace_back(rhs);
  }
};

class MulOp : public BaseOps {
 public:
  MulOp() : BaseOps("mul") {}
};

class SubOp : public BaseOps {
 public:
  SubOp() : BaseOps("sub") {}
};

class DivOp : public BaseOps {
 public:
  DivOp() : BaseOps("div") {}
};

class ModOp : public BaseOps {
 public:
  ModOp() : BaseOps("mod") {}
};

class EqualOp : public BaseOps {
 public:
  EqualOp() : BaseOps("equal") {}
};

class NotEqualOp : public BaseOps {
 public:
  NotEqualOp() : BaseOps("ne") {}
};

class LessOp : public BaseOps {
 public:
  LessOp() : BaseOps("less") {}
};

class LessEqualOp : public BaseOps {
 public:
  LessEqualOp() : BaseOps("le") {}
};

class GreaterOp : public BaseOps {
 public:
  GreaterOp() : BaseOps("greater") {}
};

class GreaterEqualOp : public BaseOps {
 public:
  GreaterEqualOp() : BaseOps("ge") {}
};

class AndOp : public BaseOps {
 public:
  AndOp() : BaseOps("and") {}
};

class OrOp : public BaseOps {
 public:
  OrOp() : BaseOps("or") {}
};

}  // namespace middleend::mlir::base

#endif