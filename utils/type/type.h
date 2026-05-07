#ifndef TYPE_TYPE_TYPE_H
#define TYPE_TYPE_TYPE_H

#include <cstddef>
#include <cstdint>
#include <string>
namespace type {
enum class TypeKind : uint8_t { UNKNOWN, BOOL, INT, FLOAT, VOID, PTR, ARRAY };

class Type {
 public:
  auto GetKind() const -> TypeKind { return kind_; }
  virtual auto ToString() const -> std::string = 0;
  Type() = delete;
  virtual ~Type() = default;
  // Added in lowering to llvmir, which is used to decide which size to align to
  virtual auto GetAlign() const -> size_t = 0;

 protected:
  explicit Type(TypeKind kind) : kind_(kind) {}

 private:
  TypeKind kind_;
};

/**
 * @brief Unknown type will be used when type haven't been determined yet or error occurs
 */
class UnknownType : public Type {
 public:
  auto ToString() const -> std::string override { return "Unknown"; }
  auto GetAlign() const -> size_t override { return 0; }
  ~UnknownType() override = default;

 private:
  UnknownType() : Type(TypeKind::UNKNOWN) {}

  friend class TypeFactory;
};

/**
 * @brief Bool type will be used when condition import logical expressions
 */
class BooleanType : public Type {
 public:
  auto ToString() const -> std::string override { return "Bool"; }
  auto GetAlign() const -> size_t override { return 1; }
  ~BooleanType() override = default;

 private:
  BooleanType() : Type(TypeKind::BOOL) {}
  friend class TypeFactory;
};

/**
 * @brief Int type will be used for integer values
 */
class IntegerType : public Type {
 public:
  auto ToString() const -> std::string override { return "Int"; }
  auto GetAlign() const -> size_t override { return 4; }
  ~IntegerType() override = default;

 private:
  IntegerType() : Type(TypeKind::INT) {}
  friend class TypeFactory;
};

/**
 * @brief Float type will be used for floating point values
 */
class FloatType : public Type {
 public:
  auto ToString() const -> std::string override { return "Float"; }
  auto GetAlign() const -> size_t override { return 4; }
  ~FloatType() override = default;

 private:
  FloatType() : Type(TypeKind::FLOAT) {}
  friend class TypeFactory;
};

/**
 * @brief Void type will be used for functions with no return value
 */
class VoidType : public Type {
 public:
  auto ToString() const -> std::string override { return "Void"; }
  auto GetAlign() const -> size_t override { return 0; }
  ~VoidType() override = default;

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
 public:
  auto ToString() const -> std::string override { return "Ptr to " + base_type_->ToString(); }
  auto GetBaseType() const -> Type * { return base_type_; }
  auto GetAlign() const -> size_t override { return 16; }
  ~PtrType() override = default;

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
 public:
  auto ToString() const -> std::string override {
    return "Array of " + element_type_->ToString() + " with size " + std::to_string(size_);
  }
  auto GetElementType() const -> Type * { return element_type_; }
  auto GetSize() const -> size_t { return size_; }
  auto GetAlign() const -> size_t override { return element_type_->GetAlign(); }
  ~ArrayType() override = default;

 private:
  explicit ArrayType(Type *element_type, size_t size)
      : Type(TypeKind::ARRAY), element_type_(element_type), size_(size) {}
  Type *element_type_;
  size_t size_;
  friend class TypeFactory;
};

}  // namespace type

#endif