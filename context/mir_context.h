#ifndef CONTEXT_MIR_CONTEXT_H
#define CONTEXT_MIR_CONTEXT_H
#include <unordered_map>
#include <vector>
#include "backend/mir/inst/inst.h"
#include "backend/mir/utils/factory/instfactory.h"
#include "backend/mir/utils/factory/opfactory.h"
#include "backend/mir/utils/memory/frameinfo.h"
#include "backend/mir/utils/operand/frameoperand.h"
#include "backend/mir/utils/operand/memoryoperand.h"
#include "backend/mir/utils/operand/operand.h"
#include "backend/mir/utils/struct/module.h"
#include "context/type_context.h"
#include "middleend/llvm/utils/operand/operand.h"
#include "mir/utils/operand/globaloperand.h"
#include "mir/utils/operand/labeloperand.h"
#include "mir/utils/operand/literaloperand.h"
#include "mir/utils/operand/regoperand.h"

namespace context {
class MIRResourceContext {
 public:
  ~MIRResourceContext();
  // Interfaces to create the operands
  // Since we will get operands by their high-level operand pointer,
  // so we will never get an operand by the id
  auto GetNewRegOperand(type::Type *type) -> backend::utils::RegOperand *;
  auto GetNewMemoryOperand() -> backend::utils::MemoryOperand *;
  auto GetFrameOperand() -> backend::utils::FrameOperand *;

  auto GetIntLiteral(int value) -> backend::utils::IntLiteral *;
  auto GetFloatLiteral(float value) -> backend::utils::FloatLiteral *;
  auto GetLabelOperand(size_t id) -> backend::utils::LabelOperand *;
  auto GetGlobalOperand(const std::string &name) -> backend::utils::GlobalOperand *;
  auto GetBoolLiteral(bool value) -> backend::utils::BoolLiteral *;
  auto GetLongLiteral(std::int64_t value) -> backend::utils::LongLiteral *;
  auto GetCharLiteral(char value) -> backend::utils::CharLiteral *;

  // When exit a function, we need to reset the resource context,
  // which will clear all the maps.
  auto ExitFunction() -> void { Reset(); }

  // Get and set the module
  auto GetModule() -> backend::utils::Module * { return module_; }
  auto SetModule(backend::utils::Module *module) -> void { module_ = module; }

  // Get the funcdef or declare instruction by the function name
  auto GetFuncDefByName(const std::string &name) -> backend::inst::FuncDefInst *;
  auto GetFuncDeclareByName(const std::string &name) -> backend::inst::FuncDeclareInst *;

  // Create Instructions
  template <typename InstType, typename... Args>
  auto CreateInstruction(Args &&...args) -> InstType * {
    auto inst = backend::utils::factory::InstFactory::CreateInst<InstType>(std::forward<Args>(args)...);
    instructions_.emplace_back(inst);
    return inst;
  }

  auto Reset() -> void {
    label_operands_.clear();
    int_literal_operands_.clear();
    float_literal_operands_.clear();
    bool_literal_operands_.clear();
    long_literal_operands_.clear();
    char_literal_operands_.clear();
    reg_num_counter_ = 0;
    frame_slot_counter_ = 0;
  }

  auto SetMemorySize(backend::utils::MemSize mem_size) -> void;

 private:
  // Used as a counter to generate new register operands,
  // and it will be reset for each function
  size_t reg_num_counter_ = 0;
  size_t frame_slot_counter_ = 0;

  backend::utils::Module *module_;

  // Map from id or value to operand
  std::unordered_map<size_t, backend::utils::LabelOperand *> label_operands_;
  std::unordered_map<std::string, backend::utils::GlobalOperand *> global_operands_;
  std::unordered_map<int, backend::utils::IntLiteral *> int_literal_operands_;
  std::unordered_map<float, backend::utils::FloatLiteral *> float_literal_operands_;
  std::unordered_map<bool, backend::utils::BoolLiteral *> bool_literal_operands_;
  std::unordered_map<std::int64_t, backend::utils::LongLiteral *> long_literal_operands_;
  std::unordered_map<char, backend::utils::CharLiteral *> char_literal_operands_;

  // FrameOperand will only be mapped by the llvmir operand,
  // and use the slot to get access to the StackSlot.

  // Record all instructions and free them when the context is destroyed
  std::vector<backend::inst::Instruction *> instructions_;

  // Record all memory operands, which will be used to set memory size
  std::vector<backend::utils::MemoryOperand *> memory_operands_;

  // A list of all operands, which will be freed when the context is destroyed
  std::vector<backend::utils::Operand *> operands_;
};

class MIRLowerContext {
 public:
  auto MapLLVMOperandToMIROperand(middleend::llvm::utils::Operand *llvm_operand,
                                  backend::utils::Operand *mir_operand) -> void {
    if (llvm_to_mir_operand_map_.count(llvm_operand) > 0) {
      throw std::runtime_error("LLVM operand has already been mapped to a MIR operand");
      return;
    }
    llvm_to_mir_operand_map_[llvm_operand] = mir_operand;
  }

  auto GetMIROperandFromLLVMOperand(middleend::llvm::utils::Operand *llvm_operand) -> backend::utils::Operand * {
    if (llvm_to_mir_operand_map_.count(llvm_operand) > 0) {
      return llvm_to_mir_operand_map_[llvm_operand];
    }
    // If the llvm operand has not been mapped, report error
    return nullptr;
  }

  auto MapFrameOperandToFrameInfo(backend::utils::FrameOperand *frame_operand,
                                  backend::utils::FrameInfo *frame_info) -> void {
    if (frame_operand_to_frame_info_map_.count(frame_operand) > 0) {
      throw std::runtime_error("Frame operand has already been mapped to a frame info");
      return;
    }
    frame_operand_to_frame_info_map_[frame_operand] = frame_info;
  }

  auto GetFrameInfoFromFrameOperand(backend::utils::FrameOperand *frame_operand) -> backend::utils::FrameInfo * {
    if (frame_operand_to_frame_info_map_.count(frame_operand) > 0) {
      return frame_operand_to_frame_info_map_[frame_operand];
    }
    // If the frame operand has not been mapped, report error
    return nullptr;
  }

  auto GetInsertPoint() -> backend::utils::Block * { return insert_point_; }
  auto SetInsertPoint(backend::utils::Block *block) -> void { insert_point_ = block; }

 private:
  // insert point
  backend::utils::Block *insert_point_;
  // Used to map the llvmir operand to mir operand
  std::unordered_map<middleend::llvm::utils::Operand *, backend::utils::Operand *> llvm_to_mir_operand_map_;

  // Map from FrameOperand to StackSlot
  std::unordered_map<backend::utils::FrameOperand *, backend::utils::FrameInfo *> frame_operand_to_frame_info_map_;
};

/**
 * @brief The MIRContext class is responsible for
 * managing the state and information related to the MIR.
 * To be honest, it is really really similar to the LLVMIRContext,
 * however, this phase is used as a bridge between the middleend and backend,
 * I hope it can be more clear and decoupled for multiple backends
 */
class MIRContext {
 public:
  // Constructor and destructor
  explicit MIRContext(TypeContext *type_context) {
    resource_context_ = new MIRResourceContext();
    lower_context_ = new MIRLowerContext();
    type_context_ = type_context;
  }

  ~MIRContext() {
    delete resource_context_;
    delete lower_context_;
  }

  auto GetResourceContext() -> MIRResourceContext * { return resource_context_; }
  auto GetLowerContext() -> MIRLowerContext * { return lower_context_; }

  // Get types
  auto GetIntegerType() -> type::Type * { return type_context_->GetIntegerType(); }
  auto GetFloatType() -> type::Type * { return type_context_->GetFloatType(); }
  auto GetBoolType() -> type::Type * { return type_context_->GetBooleanType(); }
  auto GetVoidType() -> type::Type * { return type_context_->GetVoidType(); }
  auto GetCharType() -> type::Type * { return type_context_->GetCharType(); }
  auto GetLongType() -> type::Type * { return type_context_->GetLongType(); }
  auto GetPtrType(type::Type *base_type) -> type::Type * { return type_context_->GetPtrType(base_type); }

  // Interfaces to get some resources
  auto GetNewRegOperand(type::Type *type) -> backend::utils::RegOperand * {
    return resource_context_->GetNewRegOperand(type);
  }

  // Literal values
  auto GetBoolLiteral(bool value) -> backend::utils::BoolLiteral * { return resource_context_->GetBoolLiteral(value); }
  auto GetLongLiteral(std::int64_t value) -> backend::utils::LongLiteral * {
    return resource_context_->GetLongLiteral(value);
  }
  auto GetCharLiteral(char value) -> backend::utils::CharLiteral * { return resource_context_->GetCharLiteral(value); }
  auto GetIntLiteral(int value) -> backend::utils::IntLiteral * { return resource_context_->GetIntLiteral(value); }
  auto GetFloatLiteral(float value) -> backend::utils::FloatLiteral * {
    return resource_context_->GetFloatLiteral(value);
  }
  auto GetLabelOperand(size_t id) -> backend::utils::LabelOperand * { return resource_context_->GetLabelOperand(id); }

  // Special operands
  auto GetFrameOperand() -> backend::utils::FrameOperand * { return resource_context_->GetFrameOperand(); }
  auto GetGlobalOperand(const std::string &name) -> backend::utils::GlobalOperand * {
    return resource_context_->GetGlobalOperand(name);
  }

  // Access the module
  auto GetModule() -> backend::utils::Module * { return resource_context_->GetModule(); }
  auto SetModule(backend::utils::Module *module) -> void { resource_context_->SetModule(module); }

  auto GetFuncDefByName(const std::string &name) -> backend::inst::FuncDefInst * {
    return resource_context_->GetFuncDefByName(name);
  }
  auto GetFuncDeclareByName(const std::string &name) -> backend::inst::FuncDeclareInst * {
    return resource_context_->GetFuncDeclareByName(name);
  }

 private:
  MIRResourceContext *resource_context_;
  MIRLowerContext *lower_context_;

  // Type context
  TypeContext *type_context_;
};

}  // namespace context

#endif