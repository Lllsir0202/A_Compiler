#ifndef CONTEXT_TYPE_CONTEXT_H
#define CONTEXT_TYPE_CONTEXT_H

#include <map>
#include "utils/type/type_factory.h"
namespace context {
/**
 * @brief All type related information will be controlled here
 * Anyone else cannot create Type instances directly.
 * They must go through TypeContext to get Type instances.
 */
class TypeContext {
 public:
  TypeContext() {
    // We can create some basic types here
    unknown_type_ = type::TypeFactory::GetUnknownType();
    integer_type_ = type::TypeFactory::GetIntegerType();
    float_type_ = type::TypeFactory::GetFloatType();
    void_type_ = type::TypeFactory::GetVoidType();
    boolean_type_ = type::TypeFactory::GetBooleanType();
  }

  ~TypeContext() {
    // We need to delete all created types here
    // Since in our design, no matter it is the ptr type or array type,
    // the element/base type is not owned by the ptr/array type and will always be basic types,
    // so we need to delete them after delete all ptr.
    for (auto &pair : ptr_type_map_) {
      delete pair.second;
    }
    for (auto &pair : array_type_map_) {
      delete pair.second;
    }
    delete unknown_type_;
    delete integer_type_;
    delete float_type_;
    delete void_type_;
    delete boolean_type_;
  }

  // Provide methods to get types
  auto GetUnknownType() -> type::Type * { return unknown_type_; }
  auto GetIntegerType() -> type::Type * { return integer_type_; }
  auto GetFloatType() -> type::Type * { return float_type_; }
  auto GetVoidType() -> type::Type * { return void_type_; }
  auto GetBooleanType() -> type::Type * { return boolean_type_; }
  auto GetPtrType(type::Type *base_type) -> type::Type * {
    // Check if we have already created this ptr type
    if (ptr_type_map_.find(base_type) != ptr_type_map_.end()) {
      return ptr_type_map_[base_type];
    }
    // Otherwise create a new ptr type
    type::Type *ptr_type = type::TypeFactory::GetPtrType(base_type);
    ptr_type_map_[base_type] = ptr_type;
    return ptr_type;
  }

  auto GetArrayType(type::Type *element_type, size_t size) -> type::Type * {
    // Check if we have already created this array type
    auto key = std::make_pair(element_type, size);
    if (array_type_map_.find(key) != array_type_map_.end()) {
      return array_type_map_[key];
    }
    // Otherwise create a new array type
    type::Type *array_type = type::TypeFactory::GetArrayType(element_type, size);
    array_type_map_[key] = array_type;
    return array_type;
  }

 private:
  type::Type *unknown_type_;
  type::Type *integer_type_;
  type::Type *float_type_;
  type::Type *void_type_;
  type::Type *boolean_type_;
  // following types will be created on demand, such as ptr type, we need to know the base type first
  // and we will create a map to manage them later
  /**
   * @param key: base type pointer
   * @param value: ptr type pointer
   */
  std::map<type::Type *, type::Type *> ptr_type_map_;
  /**
   * @param key: pair of element type pointer and size
   * @param value: array type pointer
   */
  std::map<std::pair<type::Type *, size_t>, type::Type *> array_type_map_;
};
}  // namespace context

#endif