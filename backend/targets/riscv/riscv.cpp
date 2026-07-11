#include "backend/targets/riscv/riscv.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
#include "backend/targets/riscv/frame/riscvframe.h"
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/isel/riscvisel.h"
#include "backend/targets/riscv/pass/branchfold.h"
#include "backend/targets/riscv/pass/machinedce.h"
#include "backend/targets/riscv/pass/peephole.h"
#include "backend/targets/riscv/printer/riscvprinter.h"
#include "backend/targets/riscv/regalloc/graphcoloralloc.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/pass/machinepassmanager.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "backend/mir/inst/memory/globaldef.h"
#include "context/compiler_context.h"

namespace backend::targetir::riscv {

auto RISCVBackend::Run() -> int {
  auto *module = compiler_context_->GetMIRContext()->GetModule();
  if (module == nullptr) {
    return 0;
  }
  RISCVISel isel(compiler_context_, &ctx_);
  GraphColorRegAlloc reg_alloc(&ctx_);
  RISCVFrame frame(&ctx_);

  // Machine-level optimization pipeline, toggled by the optimization level (like
  // the middle-end pass pipeline). Runs on the selected code before register
  // allocation, on virtual registers.
  MachinePassManager machine_passes(&ctx_);
  if (opt_level_ >= 1) {
    machine_passes.AddPass(new RISCVPeephole(&ctx_));
    machine_passes.AddPass(new MachineDCE(&ctx_));
    machine_passes.AddPass(new BranchFold(&ctx_));
  }

  for (auto *mir_func : module->GetFunctions()) {
    auto *tfunc = isel.SelectFunction(mir_func);
    if (!machine_passes.Empty()) {
      machine_passes.Run(tfunc);
    }
    auto outgoing = isel.OutgoingBytes();
    auto alloca_bytes = isel.AllocaBytes();
    // Spills sit above the outgoing-args and alloca areas.
    auto spill_slots = reg_alloc.Run(tfunc, outgoing + alloca_bytes);
    frame.Finalize(tfunc, outgoing + alloca_bytes, spill_slots, reg_alloc.UsedCalleeSaved());
    target_functions_.push_back(tfunc);
  }
  return 0;
}

auto RISCVBackend::Emit(std::ofstream *out) -> void {
  if (out == nullptr) {
    return;
  }
  // Physical registers render through the RISC-V ABI name table.
  static const RISCVPrinter PRINTER;
  TargetPhysicalReg::SetTargetPrinter(&PRINTER);

  EmitGlobals(out);

  *out << "\t.text\n";
  for (auto *func : target_functions_) {
    *out << "\t.globl " << func->GetName() << "\n";
    *out << func->GetName() << ":\n";
    for (auto *block : func->GetBlocks()) {
      *out << block->GetLabel() << ":\n";
      for (auto *inst : block->GetInsts()) {
        inst->Emit(out);
      }
    }
  }
}

auto RISCVBackend::EmitGlobals(std::ofstream *out) -> void {
  auto *module = compiler_context_->GetMIRContext()->GetModule();
  if (module == nullptr) {
    return;
  }
  for (const auto &[name, def] : module->GetGlobalDefs()) {
    // Total element count = product of array dimensions (1 for a scalar).
    size_t elems = 1;
    for (auto d : def->GetDimensions()) {
      elems *= static_cast<size_t>(d);
    }
    const auto &int_init = def->GetIntInitialValues();
    const auto &float_init = def->GetFloatInitialValues();
    // Both int and float initializers are emitted as raw 4-byte words (a float's
    // IEEE-754 bit pattern), so a single code path handles either.
    std::vector<uint32_t> words;
    words.reserve(std::max(int_init.size(), float_init.size()));
    for (int v : int_init) {
      words.push_back(static_cast<uint32_t>(v));
    }
    for (float f : float_init) {
      uint32_t bits = 0;
      std::memcpy(&bits, &f, sizeof(bits));
      words.push_back(bits);
    }
    bool zero_init = std::all_of(words.begin(), words.end(), [](uint32_t w) { return w == 0; });

    *out << "\t.globl " << name << "\n";
    if (zero_init) {
      *out << "\t.bss\n\t.align 2\n" << name << ":\n\t.zero " << elems * 4 << "\n";
    } else {
      *out << "\t.data\n\t.align 2\n" << name << ":\n";
      for (uint32_t w : words) {
        *out << "\t.word " << w << "\n";
      }
      if (words.size() < elems) {  // trailing implicit zeros
        *out << "\t.zero " << (elems - words.size()) * 4 << "\n";
      }
    }
  }
}

}  // namespace backend::targetir::riscv
