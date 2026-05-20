#ifndef CONTEXT_LLVMIR_CONTEXT_H
#define CONTEXT_LLVMIR_CONTEXT_H
#include <unordered_map>
#include "context/type_context.h"
#include "llvm/inst/inst.h"
#include "llvm/utils/operand/globaloperand.h"
#include "middleend/llvm/utils/factory/instfactory.h"
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/llvm/utils/operand/regoperand.h"
#include "middleend/llvm/utils/struct/function.h"
#include "middleend/llvm/utils/struct/module.h"
#include "middleend/mlir/utils/struct/block.h"
#include "type/type.h"

// Forward declaration
namespace middleend::llvm::inst {
class FuncDefInst;
}

namespace context {

/**
 * @brief LLVMIRResourceContext is responsible for managing the resources of llvm ir.
 */
class LLVMIRResourceContext {
 public:
  explicit LLVMIRResourceContext(TypeContext *type_context) : type_context_(type_context) {}
  ~LLVMIRResourceContext();
  /**
   * @brief Interfaces to create new operands
   */
  auto GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand *;

  auto GetNewGlobalOperand(const std::string &name, type::Type *type) -> middleend::llvm::utils::GlobalOperand *;

  /**
   * @brief Interfaces to get existing operands by id(or value for literals),
   * to ensure that the same operand will always be the same instance of operand.
   */
  auto GetRegOperand(size_t id) -> middleend::llvm::utils::RegOperand *;
  auto GetIntLiteral(int value) -> middleend::llvm::utils::IntLiteral *;
  auto GetFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral *;
  // Just for llvm intrinsic functions
  auto GetBoolLiteral(bool value) -> middleend::llvm::utils::BoolLiteral *;
  auto GetLongLiteral(std::int64_t value) -> middleend::llvm::utils::LongLiteral *;
  auto GetCharLiteral(char value) -> middleend::llvm::utils::CharLiteral *;

  auto GetGlobalOperand(const std::string &name) -> middleend::llvm::utils::GlobalOperand *;

  /**
   * @brief Label operands are used to represent the labels of basic blocks,
   * so we don't need to create new or get existing
   */
  auto GetLabelOperand(size_t id) -> middleend::llvm::utils::LabelOperand *;

  // Get a new unique block id for the new basic block
  auto GetNewBasicBlock(middleend::llvm::utils::Function *function) -> middleend::llvm::utils::BasicBlock *;

  /**
   * @brief Interfaces to manage function-related resources
   */
  auto AddFunctionDef(const std::string &func_name, middleend::llvm::inst::FuncDefInst *func_def) -> void;
  auto GetFunctionDef(const std::string &func_name) -> middleend::llvm::inst::FuncDefInst *;
  auto AddFunction(middleend::llvm::utils::Function *function) -> void;
  [[maybe_unused]] auto GetFunction(const std::string &func_name) -> middleend::llvm::utils::Function *;

  auto AddLibraryFunction(middleend::llvm::inst::FuncDeclareInst *func_def) -> void;
  auto GetLibraryFunction(const std::string &func_name) -> middleend::llvm::inst::FuncDeclareInst *;
  /**
   * @brief Interfaces to manage global variable-related resources
   */
  auto AddGlobalVarDef(const std::string &var_name, middleend::llvm::inst::GlobalDefInst *global_var_def) -> void;
  auto GetGlobalVarDef(const std::string &var_name) -> middleend::llvm::inst::GlobalDefInst *;

  /**
   * @brief Reset the reg and label id counters, while we enter a new function
   * @note Should be called when we enter a new function
   */
  auto ResetFunctionLevelResource() -> void {
    reg_id_counter_ = 0;
    label_id_counter_ = 0;
    reg_operands_.clear();
    label_operands_.clear();
  }

  auto CreateFunction(const std::string &func_name, type::Type *return_type) -> middleend::llvm::utils::Function *;

  template <typename InstType, typename... Args>
  auto CreateInst(Args &&...args) -> InstType * {
    auto *inst = middleend::llvm::utils::factory::InstFactory::CreateInst<InstType>(std::forward<Args>(args)...);
    instructions_.emplace_back(inst);
    return inst;
  }

  // Interface to get and set the top module
  auto GetModule() -> middleend::llvm::utils::Module * { return module_; }
  auto SetModule(middleend::llvm::utils::Module *module) -> void { module_ = module; }

  auto GetBlockByID(middleend::llvm::utils::Function *func, size_t id) -> middleend::llvm::utils::BasicBlock * {
    if (function_blocks_mapping_.count(func) > 0) {
      auto &blocks = function_blocks_mapping_[func];
      if (id < blocks.size()) {
        return blocks[id];
      }
    }
    // If the block does not exist, report error
    return nullptr;
  }

 private:
  // Record all instructions, and manage the memory of instructions.
  std::vector<middleend::llvm::inst::Instruction *> instructions_;

  // type context is needed for operand creation
  TypeContext *type_context_;
  // Id counters for different operand types
  size_t reg_id_counter_ = 0;
  size_t label_id_counter_ = 0;

  // Map from id to operand instance, to ensure that the same operand will be mapped to the same operand
  std::unordered_map<size_t, middleend::llvm::utils::RegOperand *> reg_operands_;
  std::unordered_map<size_t, middleend::llvm::utils::LabelOperand *> label_operands_;
  std::unordered_map<int, middleend::llvm::utils::IntLiteral *> int_literals_;
  std::unordered_map<float, middleend::llvm::utils::FloatLiteral *> float_literals_;
  std::unordered_map<std::string, middleend::llvm::utils::GlobalOperand *> global_operands_;
  // Just for llvm intrinsic functions
  std::unordered_map<bool, middleend::llvm::utils::BoolLiteral *> bool_literals_;
  std::unordered_map<std::int64_t, middleend::llvm::utils::LongLiteral *> long_literals_;
  std::unordered_map<char, middleend::llvm::utils::CharLiteral *> char_literals_;

  // Record the operands created in the context, which can be used for memory management
  std::vector<middleend::llvm::utils::Operand *> operands_;

  // Get block via label operand's id
  std::unordered_map<middleend::llvm::utils::Function *, std::vector<middleend::llvm::utils::BasicBlock *>>
      function_blocks_mapping_;

  // Top module
  middleend::llvm::utils::Module *module_;
};

/**
 * @brief LLVMIRLowerContext is responsible for lowering the base ir to llvmir
 */
class LLVMIRLowerContext {
 public:
  // Interface to get and set current insert point, which is used for instruction insertion
  auto SetInsertPoint(middleend::llvm::utils::BasicBlock *block) -> void { current_block_ = block; }
  auto GetInsertPoint() -> middleend::llvm::utils::BasicBlock * { return current_block_; }

  // Interface to get and set current function
  auto SetCurrentFunction(middleend::llvm::utils::Function *function) -> void { current_function_ = function; }
  auto GetCurrentFunction() -> middleend::llvm::utils::Function * { return current_function_; }

  // Interface to map and get llvm ops from mlir ops
  auto MapMLIRBlockToLLVMIRBlock(middleend::mlir::Block *mlir_block,
                                 middleend::llvm::utils::BasicBlock *llvm_block) -> void {
    // Should never found
    if (block_mapping_.count(mlir_block) > 0) {
      throw std::logic_error("MLIR block already mapped to LLVM IR block");
    }
    block_mapping_[mlir_block] = llvm_block;
  }
  auto GetLLVMIRBlockFromMLIRBlock(middleend::mlir::Block *mlir_block) -> middleend::llvm::utils::BasicBlock * {
    if (block_mapping_.count(mlir_block) > 0) {
      return block_mapping_[mlir_block];
    }
    return nullptr;
  }

  auto MapMLIRValueToLLVMIROperand(middleend::mlir::Value *mlir_value,
                                   middleend::llvm::utils::Operand *llvm_operand) -> void {
    // Should never found
    if (value_mapping_.count(mlir_value) > 0) {
      throw std::logic_error("MLIR value already mapped to LLVM IR operand\n");
    }
    value_mapping_[mlir_value] = llvm_operand;
  }

  auto GetLLVMIROperandFromMLIRValue(middleend::mlir::Value *mlir_value) -> middleend::llvm::utils::Operand * {
    if (value_mapping_.count(mlir_value) > 0) {
      return value_mapping_[mlir_value];
    }
    // If not found, report error
    return nullptr;
  }

 private:
  // Insert point management
  middleend::llvm::utils::BasicBlock *current_block_;
  middleend::llvm::utils::Function *current_function_;

  // Need a map from mlir block to llvmir block
  std::unordered_map<middleend::mlir::Block *, middleend::llvm::utils::BasicBlock *> block_mapping_;
  // map from mlir value to llvmir operand
  std::unordered_map<middleend::mlir::Value *, middleend::llvm::utils::Operand *> value_mapping_;
};

class LLVMIRContext {
 public:
  explicit LLVMIRContext(TypeContext *type_context) : type_context_(type_context) {
    resource_context_ = new LLVMIRResourceContext(type_context_);
    lower_context_ = new LLVMIRLowerContext();
  }

  ~LLVMIRContext() {
    delete resource_context_;
    delete lower_context_;
  }
  // Get different types of operands from the context,
  // and we will ensure that the same operand will always be the same instance of operand.
  auto GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand * {
    return resource_context_->GetNewRegOperand(type);
  }

  auto GetLabelOperand(size_t id) -> middleend::llvm::utils::LabelOperand * {
    return resource_context_->GetLabelOperand(id);
  }

  auto GetIntLiteral(int value) -> middleend::llvm::utils::IntLiteral * {
    return resource_context_->GetIntLiteral(value);
  }

  auto GetFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral * {
    return resource_context_->GetFloatLiteral(value);
  }

  auto GetBoolLiteral(bool value) -> middleend::llvm::utils::BoolLiteral * {
    return resource_context_->GetBoolLiteral(value);
  }

  auto GetLongLiteral(std::int64_t value) -> middleend::llvm::utils::LongLiteral * {
    return resource_context_->GetLongLiteral(value);
  }

  auto GetCharLiteral(char value) -> middleend::llvm::utils::CharLiteral * {
    return resource_context_->GetCharLiteral(value);
  }

  auto GetNewGlobalOperand(const std::string &name, type::Type *type) -> middleend::llvm::utils::GlobalOperand * {
    return resource_context_->GetNewGlobalOperand(name, type);
  }

  auto GetGlobalOperand(const std::string &name) -> middleend::llvm::utils::GlobalOperand * {
    return resource_context_->GetGlobalOperand(name);
  }

  // Get types
  auto GetIntegerType() -> type::Type * { return type_context_->GetIntegerType(); }
  auto GetFloatType() -> type::Type * { return type_context_->GetFloatType(); }
  auto GetBoolType() -> type::Type * { return type_context_->GetBooleanType(); }
  auto GetVoidType() -> type::Type * { return type_context_->GetVoidType(); }
  auto GetCharType() -> type::Type * { return type_context_->GetCharType(); }
  auto GetLongType() -> type::Type * { return type_context_->GetLongType(); }
  auto GetPtrType(type::Type *base_type) -> type::Type * { return type_context_->GetPtrType(base_type); }

  // Get contexts
  auto GetResourceContext() -> LLVMIRResourceContext * { return resource_context_; }
  auto GetLowerContext() -> LLVMIRLowerContext * { return lower_context_; }
  auto GetTypeContext() -> TypeContext * { return type_context_; }

  // Get module
  auto GetModule() -> middleend::llvm::utils::Module * { return resource_context_->GetModule(); }
  auto SetModule(middleend::llvm::utils::Module *module) -> void { resource_context_->SetModule(module); }

  auto GetInsertPoint() -> middleend::llvm::utils::BasicBlock * { return lower_context_->GetInsertPoint(); }
  auto SetInsertPoint(middleend::llvm::utils::BasicBlock *block) { lower_context_->SetInsertPoint(block); }

 private:
  LLVMIRLowerContext *lower_context_;
  LLVMIRResourceContext *resource_context_;

  TypeContext *type_context_;
};

}  // namespace context
#endif