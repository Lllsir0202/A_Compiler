#include "backend/targets/riscv/riscv.h"
#include "backend/targets/riscv/inst/riscvinst.h"
#include "backend/targets/riscv/isel/riscvisel.h"
#include "backend/targets/riscv/printer/riscvprinter.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "context/compiler_context.h"

namespace backend::targetir::riscv {

auto RISCVBackend::Run() -> int {
  auto *module = compiler_context_->GetMIRContext()->GetModule();
  if (module == nullptr) {
    return 0;
  }
  RISCVISel isel(compiler_context_, &ctx_);
  for (auto *mir_func : module->GetFunctions()) {
    target_functions_.push_back(isel.SelectFunction(mir_func));
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

}  // namespace backend::targetir::riscv
