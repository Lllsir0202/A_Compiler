#ifndef MIDDLEEND_IRBASE_IR_H
#define MIDDLEEND_IRBASE_IR_H
namespace middleend {
class IRVisitor;
class ConstIRVisitor;
/**
 * @brief IR base class
 * All IRs will inherit from this class,
 * and provide some common interfaces
 */
class IR {
 public:
  virtual ~IR() = default;
  /**
   * @brief LowerAble will be used to determine whether this IR
   * can be lowered to a lower level IR, over here the LLVMIR will be lowest
   */
  virtual auto LowerAble() -> bool = 0;

  /**
   * @brief accept a visitor
   * I suppose it will only be used for lowering and code generation.
   */
  virtual auto Accept(IRVisitor *visitor) -> void = 0;
  virtual auto Accept(ConstIRVisitor *visitor) const -> void = 0;
};

}  // namespace middleend

#endif