#include "backend/targets/riscv/riscv.h"

namespace backend::targetir::riscv {

auto RISCVBackend::Run() -> int {
  // Codegen pipeline (see docs/backend_codegen_plan.md). Each phase below is a
  // separate module so the stages stay decoupled and independently testable:
  //
  //   RISCVISel isel(compiler_context_);
  //   for (auto *mir_func : <MIR module functions>) {
  //     auto *tf = isel.SelectFunction(mir_func);           // -> virtual regs
  //     GraphColoringAllocator(tf, &reg_info, GPR-class).Run();
  //     GraphColoringAllocator(tf, &reg_info, FPR-class).Run();
  //     RISCVFrame().Finalize(tf);                           // PEI + ABI
  //     target_functions_.push_back(tf);
  //   }
  //
  // M0: phases are stubs; nothing is selected yet.
  (void)compiler_context_;
  return 0;
}

auto RISCVBackend::Emit(std::ofstream *out) -> void {
  if (out == nullptr) {
    return;
  }
  // M0 placeholder: proves the -S stage is wired end-to-end. Real emission walks
  // target_functions_ through RISCVPrinter once ISel/RegAlloc/Frame are filled in.
  *out << "\t.text\n";
  *out << "# RISC-V (rv64) backend skeleton -- code generation not yet implemented.\n";
  *out << "# " << target_functions_.size() << " function(s) selected.\n";
}

}  // namespace backend::targetir::riscv
