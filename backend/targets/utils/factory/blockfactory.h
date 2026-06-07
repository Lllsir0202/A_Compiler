#ifndef BACKEND_TARGETS_UTILS_FACTORY_BLOCKFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_BLOCKFACTORY_H

#include "backend/targets/utils/struct/targetblock.h"
namespace context {
class BackendContext;
}
namespace backend::targetir {
class BlockFactory {
  static auto CreateTargetBlock(size_t id) -> TargetBlock * { return new TargetBlock(id); }
  friend class context::BackendContext;
};

}  // namespace backend::targetir

#endif