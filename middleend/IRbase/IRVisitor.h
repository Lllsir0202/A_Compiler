#ifndef MIDDLEEND_IRBASE_IRVISITOR_H
#define MIDDLEEND_IRBASE_IRVISITOR_H
#include "middleend/IRbase/IR.h"
namespace middleend {
class IRVisitor {
 public:
  virtual ~IRVisitor() = default;
  virtual auto Visit(IR *ir) -> void = 0;
};

class ConstIRVisitor {
 public:
  virtual ~ConstIRVisitor() = default;
  virtual auto Visit(const IR *ir) -> void = 0;
};

template <typename Derived, typename Base = IR>
class VisitAble : Base {
 public:
  auto Accept(IRVisitor *visitor) -> void { visitor->Visit(static_cast<Derived *>(this)); }
  auto Accept(ConstIRVisitor *visitor) const -> void { visitor->Visit(static_cast<const Derived *>(this)); }
};

}  // namespace middleend

#endif