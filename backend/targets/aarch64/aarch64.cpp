#include "backend/targets/aarch64/aarch64.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include "backend/mir/inst/memory/globaldef.h"
#include "backend/targets/aarch64/frame/aarch64frame.h"
#include "backend/targets/aarch64/isel/aarch64isel.h"
#include "backend/targets/aarch64/pass/addrfold.h"
#include "backend/targets/aarch64/pass/branchfold.h"
#include "backend/targets/aarch64/pass/ifconvert.h"
#include "backend/targets/aarch64/pass/localcse.h"
#include "backend/targets/aarch64/pass/machinedce.h"
#include "backend/targets/aarch64/pass/machinelicm.h"
#include "backend/targets/aarch64/pass/peephole.h"
#include "backend/targets/aarch64/pass/slpvectorize.h"
#include "backend/targets/aarch64/printer/aarch64printer.h"
#include "backend/targets/aarch64/regalloc/aarch64alloc.h"
#include "backend/targets/utils/operand/targetreg.h"
#include "backend/targets/utils/pass/machinepassmanager.h"
#include "backend/targets/utils/struct/targetblock.h"
#include "backend/targets/utils/struct/targetfunction.h"
#include "context/compiler_context.h"

namespace backend::targetir::aarch64 {

auto AArch64Backend::Run() -> int {
  auto *module = compiler_context_->GetMIRContext()->GetModule();
  if (module == nullptr) {
    return 0;
  }
  AArch64ISel isel(compiler_context_, &ctx_, magic_div_);
  AArch64GraphColorRegAlloc reg_alloc(&ctx_);
  AArch64Frame frame(&ctx_);

  // Machine-level optimization pipeline, toggled by the optimization level (like
  // the middle-end pass pipeline). Runs on the selected code before register
  // allocation, on virtual registers.
  MachinePassManager machine_passes(&ctx_);
  if (opt_level_ >= 1) {
    machine_passes.AddPass(new AArch64IfConvert(&ctx_));
    // Keep tree combines after SLP: rewriting mul/add or shift/add too early
    // destroys the scalar expression shape from which vector packs are built.
    machine_passes.AddPass(new AArch64Peephole(&ctx_, false));
    machine_passes.AddPass(new AArch64AddrFold(&ctx_));
    // Address folding leaves its now-dead address-building chain behind. Remove
    // it before SLP so adjacent scalar memory/arithmetic lanes become visible
    // to the packer; a second DCE below cleans up the vector rewrite itself.
    machine_passes.AddPass(new AArch64MachineDCE(&ctx_));
    machine_passes.AddPass(new AArch64SLPVectorize(&ctx_));
    machine_passes.AddPass(new AArch64Peephole(&ctx_));
    machine_passes.AddPass(new AArch64LocalCSE(&ctx_));
    machine_passes.AddPass(new AArch64MachineLICM(&ctx_));
    machine_passes.AddPass(new AArch64MachineDCE(&ctx_));
    machine_passes.AddPass(new AArch64BranchFold(&ctx_));
  }

  AArch64PostRAAddrFold post_ra_addr_fold(&ctx_);

  for (auto *mir_func : module->GetFunctions()) {
    auto *tfunc = isel.SelectFunction(mir_func);
    if (!machine_passes.Empty()) {
      machine_passes.Run(tfunc);
    }
    auto outgoing = isel.OutgoingBytes();
    auto alloca_bytes = isel.AllocaBytes();
    auto spill_slots = reg_alloc.Run(tfunc, outgoing + alloca_bytes);
    if (opt_level_ >= 1 && std::getenv("AARCH64_POSTRA_ADDR_FOLD_OFF") == nullptr) {
      // Allocation can make a sign-extension temporary die at its sole
      // register-offset load/store. Fold it only now so the shorter physical
      // sequence cannot perturb virtual-register liveness or coloring.
      post_ra_addr_fold.RunOnFunction(tfunc);
    }
    frame.Finalize(tfunc, outgoing + alloca_bytes, spill_slots, reg_alloc.UsedCalleeSaved());
    target_functions_.push_back(tfunc);
  }
  if (std::getenv("AARCH64_POSTRA_ADDR_FOLD_STATS") != nullptr) {
    std::cerr << "AArch64PostRAAddrFold: folded=" << post_ra_addr_fold.FoldedCount() << '\n';
  }
  return 0;
}

auto AArch64Backend::Emit(std::ofstream *out) -> void {
  if (out == nullptr) {
    return;
  }
  static const AArch64Printer PRINTER;
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

auto AArch64Backend::EmitGlobals(std::ofstream *out) -> void {
  auto *module = compiler_context_->GetMIRContext()->GetModule();
  if (module == nullptr) {
    return;
  }
  for (const auto &[name, def] : module->GetGlobalDefs()) {
    size_t elems = 1;
    for (auto d : def->GetDimensions()) {
      elems *= static_cast<size_t>(d);
    }
    const auto &int_init = def->GetIntInitialValues();
    const auto &float_init = def->GetFloatInitialValues();
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
      if (words.size() < elems) {
        *out << "\t.zero " << (elems - words.size()) * 4 << "\n";
      }
    }
  }
}

}  // namespace backend::targetir::aarch64
