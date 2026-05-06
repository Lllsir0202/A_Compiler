#ifndef MIDDLEEND_LLVM_UTILS_FACTORY_BLOCKFACTORY_H
#define MIDDLEEND_LLVM_UTILS_FACTORY_BLOCKFACTORY_H

#include "middleend/llvm/utils/struct/block.h"
// Forward declare
namespace context {
class LLVMIRResourceContext;
}  // namespace context

namespace middleend::llvm::utils::factory {
class BlockFactory {
 private:
  static auto CreateBlock(size_t id) -> BasicBlock * { return new BasicBlock(id); }
  friend class context::LLVMIRResourceContext;
};

}  // namespace middleend::llvm::utils::factory

#endif