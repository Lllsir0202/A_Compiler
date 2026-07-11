#ifndef BACKEND_TARGETS_UTILS_CONTEXT_TARGETCONTEXT_H
#define BACKEND_TARGETS_UTILS_CONTEXT_TARGETCONTEXT_H

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "backend/targets/utils/inst/targetinst.h"
#include "backend/targets/utils/operand/targetliteral.h"
#include "backend/targets/utils/operand/targetoperand.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/operand/targetsymbol.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"

namespace backend::targetir {

/**
 * @brief Owning factory for the target IR (operands, instructions, blocks,
 * functions). Every object created through it is heap-allocated and kept alive
 * for the lifetime of the context, so the backend never dangles a pointer and
 * cleanup is a single destructor. Target-independent: instructions are created
 * through the CreateInst<T> template so this class need not know concrete ISAs.
 */
class TargetContext {
 public:
  // --- Registers ---
  auto NewVirtualReg(TargetRegClass reg_class, TargetRegSize reg_size) -> TargetVirtualReg * {
    auto *reg = new TargetVirtualReg(reg_class, reg_size, next_vreg_id_++);
    operands_.emplace_back(reg);
    return reg;
  }

  auto PhysReg(TargetRegClass reg_class, TargetRegSize reg_size, uint8_t id) -> TargetPhysicalReg * {
    auto *reg = new TargetPhysicalReg(reg_size, reg_class, id);
    operands_.emplace_back(reg);
    return reg;
  }

  // --- Immediates / labels / symbols ---
  auto Imm(int value) -> TargetIntLiteral * {
    auto *imm = new TargetIntLiteral(value);
    operands_.emplace_back(imm);
    return imm;
  }

  auto Label(std::string name) -> TargetLabel * {
    auto *label = new TargetLabel(std::move(name));
    operands_.emplace_back(label);
    return label;
  }

  auto Symbol(std::string name) -> TargetSymbol * {
    auto *sym = new TargetSymbol(std::move(name));
    operands_.emplace_back(sym);
    return sym;
  }

  // --- Instructions (concrete ISA type supplied by the caller) ---
  template <typename Inst, typename... Args>
  auto CreateInst(Args &&...args) -> Inst * {
    auto *inst = new Inst(std::forward<Args>(args)...);
    insts_.emplace_back(inst);
    return inst;
  }

  // --- Structural ---
  auto NewBlock(size_t id) -> TargetBlock * {
    auto *block = new TargetBlock(id);
    blocks_.emplace_back(block);
    return block;
  }

  auto NewFunction(const std::string &name) -> TargetFunction * {
    auto *func = new TargetFunction(name);
    functions_.emplace_back(func);
    return func;
  }

 private:
  size_t next_vreg_id_ = 0;
  std::vector<std::unique_ptr<TargetOperand>> operands_;
  std::vector<std::unique_ptr<TargetInst>> insts_;
  std::vector<std::unique_ptr<TargetBlock>> blocks_;
  std::vector<std::unique_ptr<TargetFunction>> functions_;
};

}  // namespace backend::targetir

#endif
