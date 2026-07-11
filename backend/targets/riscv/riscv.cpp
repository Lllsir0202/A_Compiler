#include "backend/targets/riscv/riscv.h"
#include <algorithm>
#include "backend/targets/riscv/frame/riscvframe.h"
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/isel/riscvisel.h"
#include "backend/targets/riscv/printer/riscvprinter.h"
#include "backend/targets/riscv/regalloc/naivealloc.h"
#include "backend/targets/utils/operand/targetreg.h"
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
  NaiveRegAlloc reg_alloc(&ctx_);
  RISCVFrame frame(&ctx_);
  for (auto *mir_func : module->GetFunctions()) {
    auto *tfunc = isel.SelectFunction(mir_func);
    auto alloca_bytes = isel.AllocaBytes();
    auto spill_slots = reg_alloc.Run(tfunc, alloca_bytes);
    frame.Finalize(tfunc, alloca_bytes, spill_slots);
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
    bool zero_init = int_init.empty() || std::all_of(int_init.begin(), int_init.end(), [](int v) { return v == 0; });

    *out << "\t.globl " << name << "\n";
    if (zero_init) {
      *out << "\t.bss\n\t.align 2\n" << name << ":\n\t.zero " << elems * 4 << "\n";
    } else {
      *out << "\t.data\n\t.align 2\n" << name << ":\n";
      for (int v : int_init) {
        *out << "\t.word " << v << "\n";
      }
      if (int_init.size() < elems) {  // trailing implicit zeros
        *out << "\t.zero " << (elems - int_init.size()) * 4 << "\n";
      }
    }
  }
}

}  // namespace backend::targetir::riscv
