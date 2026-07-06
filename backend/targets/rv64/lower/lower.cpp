#include "backend/targets/rv64/lower/lower.h"

#include <exception>
#include <string>

#include "backend/mir/utils/struct/block.h"
#include "backend/mir/utils/struct/function.h"
#include "backend/mir/utils/struct/module.h"
#include "context/backend_context.h"
#include "context/compiler_context.h"
#include "context/mir_context.h"

namespace backend::targetir::rv64 {

auto RV64Lower::Lower() -> int {
  try {
    auto *backend_context = compiler_context_->GetBackendContext();
    auto *mir_module = compiler_context_->GetMIRContext()->GetModule();
    if (mir_module == nullptr) {
      throw std::runtime_error("RV64 lower received a null MIR module");
    }

    auto *target_module = backend_context->CreateModule();
    backend_context->SetModule(target_module);

    for (auto *mir_function : mir_module->GetFunctions()) {
      auto *target_function = backend_context->CreateFunction(mir_function->GetName());
      backend_context->SetCurrentFunction(target_function);
      backend_context->ClearBlockMap();

      for (auto *mir_block : mir_function->GetBlocks()) {
        auto *target_block = backend_context->CreateBlock(mir_block->GetId());
        target_block->SetFuncName(target_function->GetName());
        target_function->AddBlock(target_block);
        backend_context->MapBlock(mir_block->GetId(), target_block);
      }

      for (auto *mir_block : mir_function->GetBlocks()) {
        auto *target_block = backend_context->GetMappedBlock(mir_block->GetId());
        if (target_block == nullptr) {
          throw std::runtime_error("RV64 lower failed to find mapped target block");
        }
        backend_context->SetCurrentBlock(target_block);
        for (auto *mir_inst : mir_block->GetInstructions()) {
          lower_helper_->LowerDispatch(mir_inst);
        }
      }
    }
  } catch (const std::exception &e) {
    compiler_context_->Print("Error during RV64 lowering: " + std::string(e.what()));
    return -1;
  }

  return 0;
}

}  // namespace backend::targetir::rv64
