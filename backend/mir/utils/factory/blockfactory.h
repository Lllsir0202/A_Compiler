#ifndef BACKEND_MIR_UTILS_FACTORY_BLOCKFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_BLOCKFACTORY_H

#include "backend/mir/utils/struct/block.h"
namespace backend::utils::factory {
class BlockFactory {
  static auto CreateBlock(size_t id) -> Block * { return new Block(id); }
};
}  // namespace backend::utils::factory
#endif