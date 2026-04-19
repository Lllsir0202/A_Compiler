#ifndef MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#define MIDDLEEND_MLIR_DIALECT_BASE_BINARYOP_H
#include "middleend/mlir/dialect/base/basic.h"
namespace middleend::mlir::base {
class AddOp : public BaseOps {
 public:
  AddOp() : BaseOps("add") {}
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