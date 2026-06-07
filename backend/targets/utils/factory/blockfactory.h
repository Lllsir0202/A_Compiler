#ifndef BACKEND_TARGETS_UTILS_FACTORY_BLOCKFACTORY_H
#define BACKEND_TARGETS_UTILS_FACTORY_BLOCKFACTORY_H

#include "backend/targets/utils/struct/targetblock.h"
namespace backend::targetir {
class BlockFactory {
  static auto CreateTargetBlock(size_t id) -> TargetBlock * { return new TargetBlock(id); }
};

}  // namespace backend::targetir

#endif