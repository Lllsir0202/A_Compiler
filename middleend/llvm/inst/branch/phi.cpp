#include "middleend/llvm/inst/branch/phi.h"
#include "context/compiler_context.h"
#include "llvm/utils/operand/operand.h"
#include "type/type.h"

namespace middleend::llvm::inst {
auto PhiInst::Emit(std::ofstream *out) const -> void {
  // Emit the phi instruction in LLVM IR format
  *out << "  ";
  result_->Emit(out);
  *out << " = phi " << utils::Operand::GetTypeInLLVMIR(result_->GetType()) << " ";
  for (size_t i = 0; i < operands_.size(); ++i) {
    *out << "[ ";
    operands_[i]->Emit(out);
    *out << ", ";
    incoming_blocks_[i]->Emit(out);
    *out << " ]";
    if (i != operands_.size() - 1) {
      *out << ", ";
    }
  }
  *out << "\n";
}

auto PhiInst::Verify(context::CompilerContext *context) const -> bool {
  // Verify the phi instruction
  // Check if the number of incoming values and blocks are the same
  if (operands_.size() != incoming_blocks_.size()) {
    context->Print("Mismatched number of incoming values and blocks in phi instruction\n");
    return false;
  }
  // Check if the types of incoming values are consistent with the result type
  for (const auto &value : operands_) {
    if (value == nullptr) {
      context->Print("Null incoming value in phi instruction\n");
      return false;
    }
    if (value->GetType() != result_->GetType()) {
      context->Print("Inconsistent types in phi instruction\n");
      return false;
    }
  }
  return true;
}

auto PhiInst::Create(context::LLVMIRContext *context, std::vector<utils::Operand *> incoming_values,
                     std::vector<utils::LabelOperand *> incoming_blocks, type::Type *result_type) -> PhiInst * {
  auto *inst = new PhiInst();
  for (size_t i = 0; i < incoming_values.size(); ++i) {
    inst->AddIncoming(incoming_values[i], incoming_blocks[i]);
  }
  auto res = context->GetNewRegOperand(result_type);
  inst->SetResult(res);
  res->SetInstruction(inst);
  return inst;
}

}  // namespace middleend::llvm::inst