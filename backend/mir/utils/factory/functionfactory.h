#ifndef BACKEND_MIR_UTILS_FACTORY_FUNCTIONFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_FUNCTIONFACTORY_H
#include "backend/mir/utils/memory/frameinfo.h"
#include "backend/mir/utils/struct/function.h"
namespace context {
class MIRContext;
}
namespace backend::utils::factory {
class FunctionFactory {
 public:
  static auto CreateFunction(const std::string &name, type::Type *return_type) -> Function * {
    return new Function(name, return_type);
  }

  static auto CreateFrameInfo(size_t frame_id) -> FrameInfo * { return new FrameInfo(frame_id); }

  friend class context::MIRContext;
};
}  // namespace backend::utils::factory

#endif