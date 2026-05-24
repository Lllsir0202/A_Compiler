#ifndef BACKEND_MIR_UTILS_FACTORY_BLOCKFACTORY_H
#define BACKEND_MIR_UTILS_FACTORY_BLOCKFACTORY_H

#include "backend/mir/utils/struct/block.h"
namespace context {
class MIRContext;
}
namespace backend::utils::factory {
class BlockFactory {
  static auto CreateBlock(size_t id) -> Block * { return new Block(id); }

  friend class context::MIRContext;
};
}  // namespace backend::utils::factory
#endif