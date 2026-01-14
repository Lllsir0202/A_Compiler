#ifndef TYPE_TYPE_TYPE_H
#define TYPE_TYPE_TYPE_H

#include <cstddef>
namespace type {
enum class TypeKind { UNKNOWN, BOOL, INT, FLOAT, VOID, PTR, ARRAY };

class Type {
 public:
  auto GetKind() const -> TypeKind { return kind_; }
  Type() = delete;

 protected:
  explicit Type(TypeKind kind) : kind_(kind) {}

 private:
  TypeKind kind_;
};

/**
 * @brief Unknown type will be used when type haven't been determined yet or error occurs
 */
class UnknownType : public Type {
 private:
  UnknownType() : Type(TypeKind::UNKNOWN) {}

  friend class TypeFactory;
};

/**
 * @brief Bool type will be used when condition import logical expressions
 */
class BoolenType : public Type {
 private:
  BoolenType() : Type(TypeKind::BOOL) {}

  friend class TypeFactory;
};

/**
 * @brief Int type will be used for integer values
 */
class IntegarType : public Type {
 private:
  IntegarType() : Type(TypeKind::INT) {}

  friend class TypeFactory;
};

/**
 * @brief Float type will be used for floating point values
 */
class FloatType : public Type {
 private:
  FloatType() : Type(TypeKind::FLOAT) {}

  friend class TypeFactory;
};

/**
 * @brief Void type will be used for functions with no return value
 */
class VoidType : public Type {
 private:
  VoidType() : Type(TypeKind::VOID) {}

  friend class TypeFactory;
};

/**
 * @brief Ptr type will be used to pass parameter which is initially an array
 * And here we need to record the base type of the pointer
 * @brief Since we don't compare dimensions, so we only need to record the base type of an array,
 * which means an array of any dimension with the same base type will be considered as the same type
 * as a function parameter.
 */
class PtrType : public Type {
 private:
  explicit PtrType(Type *base_type) : Type(TypeKind::PTR), base_type_(base_type) {}
  Type *base_type_;
  friend class TypeFactory;
};

/**
 * @brief Array type will be used for array variables
 * And here we need to record the element type and size of the array
 * @brief Since we need to use the size information when allocating space for arrays
 */
class ArrayType : public Type {
 private:
  explicit ArrayType(Type *element_type, size_t size)
      : Type(TypeKind::ARRAY), element_type_(element_type), size_(size) {}
  Type *element_type_;
  size_t size_;
  friend class TypeFactory;
};

}  // namespace type

#endif