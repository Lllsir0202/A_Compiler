#include "backend/targets/rv64/lower/lower.h"

#include <exception>
#include <string>

#include "backend/mir/inst/memory/globaldef.h"
#include "backend/mir/utils/struct/block.h"
#include "backend/mir/utils/struct/function.h"
#include "backend/mir/utils/struct/module.h"
#include "backend/targets/rv64/symbol/rv64global.h"
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

    // Register library functions, which are used for linking and code generation
    for (const auto &[name, lib_func] : mir_module->GetFuncDeclares()) {
      if (llvm_library_functions_.count(name) > 0) {
        continue;
      }
      target_module->AddLibraryFunction(name);
    }

    for (const auto &[_, global_def] : mir_module->GetGlobalDefs()) {
      if (!global_def->GetIntInitialValues().empty()) {
        target_module->AddGlobalObject(
            new RV64GlobalObject(global_def->GetName(), global_def->GetAllocatedType(), global_def->GetDimensions(),
                                 global_def->GetIntInitialValues(), global_def->IsConstant()));
      } else {
        target_module->AddGlobalObject(
            new RV64GlobalObject(global_def->GetName(), global_def->GetAllocatedType(), global_def->GetDimensions(),
                                 global_def->GetFloatInitialValues(), global_def->IsConstant()));
      }
    }

    for (auto *mir_function : mir_module->GetFunctions()) {
      auto *target_function = backend_context->CreateFunction(mir_function->GetName());
      backend_context->SetCurrentFunction(target_function);
      backend_context->ClearBlockMap();

      // Create the frame object according to the frame objects in the MIR function
      target_function->ReserveFrameObject(mir_function->GetFrameInfosSize());
      for (const auto *frame_info : mir_function->GetFrameInfos()) {
        backend_context->AddFrameObject(frame_info->GetSize(), frame_info->GetAlign());
      }

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

      // Before exiting the function, we need to clear the current function and current block in the backend context
      // Also need to clear the block map
      backend_context->SetCurrentFunction(nullptr);
      backend_context->SetCurrentBlock(nullptr);
      backend_context->ClearBlockMap();
    }
  } catch (const std::exception &e) {
    compiler_context_->Print("Error during RV64 lowering: " + std::string(e.what()));
    return -1;
  }

  return 0;
}

}  // namespace backend::targetir::rv64
