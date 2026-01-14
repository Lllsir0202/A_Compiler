#ifndef TYPE_TYPE_FACTORY_H
#define TYPE_TYPE_FACTORY_H

#include "utils/type/type.h"

namespace type {

/**
 * @brief TypeFactory is responsible for creating Type pointer.
 * But pay attention, we don't do management via TypeFactory,
 * the resource management is done by TypeContext.
 * So here we only provide static methods to get Type pointers.
 */
class TypeFactory {
 public:
  static auto GetUnknownType() -> Type * { return new UnknownType(); }
  static auto GetBoolenType() -> Type * { return new BoolenType(); }
  static auto GetIntegarType() -> Type * { return new IntegarType(); }
  static auto GetFloatType() -> Type * { return new FloatType(); }
  static auto GetVoidType() -> Type * { return new VoidType(); }
  static auto GetPtrType(Type *base_type) -> Type * { return new PtrType(base_type); }
  static auto GetArrayType(Type *element_type, size_t size) -> Type * { return new ArrayType(element_type, size); }
};

}  // namespace type

#endif