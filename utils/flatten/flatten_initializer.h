#ifndef UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#define UTILS_FLATTEN_FLATTEN_INITIALIZER_H
#include <vector>
#include "ast/ast_decl.h"
#include "context/compiler_context.h"
#include "utils/flatten/initializer.h"
#include "utils/flatten/utils.h"
namespace frontend::ast {
class Initializer;
class InitValNode;
class InitListNode;
}  // namespace frontend::ast

namespace utils::flatten {

class FlattenHelper {
 public:
  explicit FlattenHelper(context::CompilerContext *compiler_context) : compiler_context_(compiler_context) {}
  ~FlattenHelper() { compiler_context_ = nullptr; }

 private:
  auto GetElement(const utils::flatten::Object &obj) -> Object {
    if (obj.IsScalar()) {
      return Object({});
    }
    std::vector<int> sub_object = obj.current_initialized_object_;
    sub_object.pop_back();
    return Object(sub_object);
  }

  auto GetArraySize(const Object &obj) -> int {
    if (obj.IsScalar()) {
      return -1;  // means it is a scalar, not an array
    }
    return obj.current_initialized_object_.back();
  }

  auto MakeSubCursor(frontend::ast::InitListNode *init_list) -> InitCursor {
    std::vector<frontend::ast::Initializer *> sub_inits;
    sub_inits.reserve(init_list->init_values_.size());
    for (const auto &init : init_list->init_values_) {
      sub_inits.emplace_back(init.get());
    }
    return InitCursor(sub_inits);
  }

  auto MakeSubCursor(frontend::ast::InitValNode *init_val) -> InitCursor {
    std::vector<frontend::ast::Initializer *> sub_inits;
    sub_inits.emplace_back(init_val);
    return InitCursor(sub_inits);
  }

  auto ZeroFill(const Object &obj, const Address &address, initializer::ArrayInitializer &results) -> void {
    if (obj.IsScalar()) {
      results.AddInitializer(nullptr, address);
    } else {
      int array_size = GetArraySize(obj);
      Object sub_obj = GetElement(obj);
      for (int i = 0; i < array_size; i++) {
        Address sub_address = address;
        sub_address.address_.emplace_back(i);
        ZeroFill(sub_obj, sub_address, results);
      }
    }
  }

  using LeafHandle = initializer::InitializerPtr(frontend::ast::Initializer *init_node);

  void InitObject(const Object &obj, InitCursor &cursor, Address address, initializer::ArrayInitializer &results,
                  LeafHandle function_pointer);
  /**
   * @brief handle the initializer list is too deep
   * @use: just get the first scalar initializer in the deep loop
   * @use: cursor will be set to the end of the current init list,
   * since we have already get the first scalar
   * initializer in the current init list
   */
  auto CastInitializer(frontend::ast::Initializer *init_node, LeafHandle function_pointer)
      -> initializer::InitializerPtr;

 public:
  /**
   * @brief Flatten the initializer list to a single value, used for constant expression evaluation
   * e.g., int a[2] = {1, 2}; -> int a[2] = 1, 2;
   * @param init_node: the initializer node to be flattened
   * @param dims&: the dimensions of the array, used for calculating the total size of the array
   * @param depth: the current depth of the initializer list
   * @param function_pointer: the function pointer used for figure out how to get the value.
   * @return the flattened initializer
   */
  auto FlattenInitializer(frontend::ast::Initializer *init_node, std::vector<int> &dims, LeafHandle function_pointer)
      -> initializer::ArrayInitializer;

 private:
  context::CompilerContext *compiler_context_;
};

}  // namespace utils::flatten

#endif