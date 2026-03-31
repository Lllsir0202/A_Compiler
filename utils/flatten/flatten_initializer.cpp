#include "utils/flatten/flatten_initializer.h"
#include <memory>
#include "frontend/ast/ast_decl.h"
#include "utils/flatten/initializer.h"
#include "utils/flatten/utils.h"

namespace utils::flatten {
/**
 * @brief get the first scalar initializer in the deep loop
 * @use: handle the initializer list which is too deep, just get the first scalar initializer
 * @example:
 * in our current design, the object represent which we need to initialize
 *
 * for instance, we have an object [3], and the initializer list is {{1, 2}, 3 };
 * then we will go on to initialize the three object [] with {1, 2}, 3,
 * but the {1, 2} is not a scalar initializer, which means we need to cast it to a scalar initializer
 * so over here, we will just get the first scalar initializer in the initial_list,
 * no matter how deep the initial_list is.
 */
auto FlattenHelper::CastInitializer(frontend::ast::Initializer *init_node,
                                    LeafHandle function_pointer) -> initializer::InitializerPtr {
  while (init_node != nullptr && !init_node->IsScalar()) {
    // since the init_node is NOT scalar, it must be an init list
    auto init_list = static_cast<frontend::ast::InitListNode *>(init_node);
    if (!init_list->init_values_.empty()) {
      init_node = init_list->init_values_[0].get();
      continue;
    }
    // if it is empty, we can just fill the default value, which is nullptr
    init_node = nullptr;
  }
  // means we have reached the end of the deep loop
  return function_pointer(init_node);
}

void FlattenHelper::InitObject(const Object &obj, InitCursor &cursor, Address address,
                               initializer::ArrayInitializer &results, LeafHandle function_pointer) {
  // Implementation for initializing the object
  if (obj.IsScalar()) {
    // means it has reached the end of the array
    // so if the cursor has more initializer, we can just ignore the extra initializer
    auto init_node = cursor.Next();
    if (init_node != nullptr) {
      if (init_node->IsScalar()) {
        // means it is a scalar
        results.AddInitializer(function_pointer(init_node), address);
      }
      // means it is an init list, but the array has no more dimension,
      // so we will just get the first scalar initializer in the init list
      else {
        compiler_context_->ReportWarning(init_node, "Initializing a scalar with a list");
        results.AddInitializer(CastInitializer(init_node, function_pointer), address);
        cursor.MarkAllConsumed();
      }
    } else {
      // means there is no more initializer, we will just fill the default value
      /**
       * @note: while in initializing the results, it is defaultly nullptr which is default value.
       */
      results.AddInitializer(nullptr, address);
    }

  } else {
    // it is a list
    int array_size = GetArraySize(obj);
    Object sub_obj = GetElement(obj);

    for (int i = 0; i < array_size; i++) {
      auto init_node = cursor.Peek();
      if (init_node != nullptr && !init_node->IsScalar()) {
        // means it is an init list, we will enter the next level of the loop
        cursor.Next();
        InitCursor sub_cursor = MakeSubCursor(static_cast<frontend::ast::InitListNode *>(init_node));
        InitObject(sub_obj, sub_cursor, address[i], results, function_pointer);
        if (sub_cursor.HasMore()) {
          compiler_context_->ReportError(init_node, "Too many initializers for array");
          return;
        }
      } else if (init_node != nullptr) {
        // if it is not nullptr and it is a scalar
        /**
         * @example int a[3][3] = {1, 2, {3, 4}};
         * when we are initializing the a[0], we will meet the 1, 2, {3, 4}
         * since `1` is a scalar, we cannot use it to initialize the a[0],
         * so we will just get the next scalar initializers to initialize the a[0][0], a[0][1] and a[0][2].
         * over here, the implement which makes it works is :
         *
         * take the [10] for example, the sub_obj is the element type of the obj, which is
         * means
         */
        InitObject(sub_obj, cursor, address[i], results, function_pointer);
      } else {
        // means there is no more initializer, we will just fill the default value
        /**
         * @note: while in initializing the results, it is defaultly nullptr which is default value.
         */
        ZeroFill(sub_obj, address[i], results);
      }
    }
  }
}

/**
 * @brief Flatten the initializer list
 * @example:
 * int a[3][3] = { 1, 2, {3, 4}, 5, 6};
 * typically, we initialize the `a` with the whole initializer list { 1, 2, ...}
 * then initalizer the a[0] with the pair-of-braces initializer list such as {1,2,3}
 * but we cannot meet the pair-of-braces initializer list, since we only have the scalar: 1,2
 * so we need to initializer a[0] with the first 3 initializers: 1, 2, {3, 4}
 * however, the {3, 4} is not a scalar initializer, so we need to get the first scalar to fill
 *
 * which means, we need to start from the a pointer, if there is the pair-of-braces,
 * then we will initialize the:
 * topest dimension which in the example is a[0],a[1],a[2] with the following three pair-of-braces.
 * if there is, we will initialize the a[0][0], a[0][1]... with the scalars in the three pair-of-braces
 * if there is no any, we will initialize the a[i] with the following `dim` scalars.
 *
 * @note: when analysing a variable array in the semantic analysis,
 * we will use a sepcial function pointer to just check whether the initializer is overflow or not,
 * since we do not need to ( and cannot as well) get the value of the initializer.
 */
auto FlattenHelper::FlattenInitializer(frontend::ast::Initializer *init_node, std::vector<int> &dims,
                                       LeafHandle function_pointer) -> initializer::ArrayInitializer {
  // Create the results array
  initializer::ArrayInitializer results;
  // Create the initial object
  auto reversed_dims = std::vector<int>(dims.rbegin(), dims.rend());
  Object obj(reversed_dims);

  // Create the initial cursor
  std::vector<frontend::ast::Initializer *> init_list;
  if (!init_node->IsScalar()) {
    auto init_list_node = static_cast<frontend::ast::InitListNode *>(init_node);
    for (const auto &init : init_list_node->init_values_) {
      init_list.emplace_back(init.get());
    }
  }
  InitCursor cursor(init_list);
  Address address(dims);

  // Initialize the object
  InitObject(obj, cursor, address, results, function_pointer);
  if (cursor.HasMore()) {
    compiler_context_->ReportError(cursor.Peek(), "Too many initializers for array");
  }
  // Now the results array has been initialized with the flattened initializer list
  return results;
}
}  // namespace utils::flatten