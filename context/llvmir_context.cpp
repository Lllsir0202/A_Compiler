#include "context/llvmir_context.h"
#include <cassert>
#include <stdexcept>
#include "llvm/inst/func/funcdef.h"
#include "llvm/utils/factory/instfactory.h"
#include "middleend/llvm/utils/factory/blockfactory.h"
#include "middleend/llvm/utils/factory/funcfactory.h"
#include "middleend/llvm/utils/factory/opfactory.h"
#include "middleend/llvm/utils/struct/block.h"
#include "middleend/llvm/utils/struct/function.h"

namespace context {
LLVMIRResourceContext::~LLVMIRResourceContext() {
  // Delete all the operands in the context
  for (auto &[_, op] : reg_operands_) {
    delete op;
  }
  for (auto &[_, op] : label_operands_) {
    delete op;
  }
  for (auto &[_, op] : int_literals_) {
    delete op;
  }
  for (auto &[_, op] : float_literals_) {
    delete op;
  }
  for (auto &[_, op] : global_operands_) {
    delete op;
  }
  for (auto &[_, op] : bool_literals_) {
    delete op;
  }
  for (auto &[_, op] : long_literals_) {
    delete op;
  }
  for (auto &[_, op] : char_literals_) {
    delete op;
  }
  for (auto &inst : instructions_) {
    delete inst;
  }
  instructions_.clear();
}

auto LLVMIRResourceContext::GetNewRegOperand(type::Type *type) -> middleend::llvm::utils::RegOperand * {
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateRegOperand(reg_id_counter_++);
  op->SetType(type);
  return op;
}

auto LLVMIRResourceContext::GetNewGlobalOperand(const std::string &name,
                                                type::Type *type) -> middleend::llvm::utils::GlobalOperand * {
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateGlobalOperand(name);
  op->SetType(type);
  return op;
}

// Get existing operands by id(or value for literals)
auto LLVMIRResourceContext::GetRegOperand(size_t id) -> middleend::llvm::utils::RegOperand * {
  if (reg_operands_.count(id) > 0) {
    return reg_operands_[id];
  }
  // If the operand does not exist, report error
  return nullptr;
}

auto LLVMIRResourceContext::GetLabelOperand(size_t id) -> middleend::llvm::utils::LabelOperand * {
  if (label_operands_.count(id) > 0) {
    return label_operands_[id];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateLabelOperand(id);
  label_operands_[id] = op;
  return op;
}

auto LLVMIRResourceContext::GetIntLiteral(int value) -> middleend::llvm::utils::IntLiteral * {
  if (int_literals_.count(value) > 0) {
    return int_literals_[value];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateIntLiteral(value);
  op->SetType(type_context_->GetIntegerType());
  int_literals_[value] = op;
  return op;
}

auto LLVMIRResourceContext::GetFloatLiteral(float value) -> middleend::llvm::utils::FloatLiteral * {
  if (float_literals_.count(value) > 0) {
    return float_literals_[value];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateFloatLiteral(value);
  op->SetType(type_context_->GetFloatType());
  float_literals_[value] = op;
  return op;
}

auto LLVMIRResourceContext::GetBoolLiteral(bool value) -> middleend::llvm::utils::BoolLiteral * {
  if (bool_literals_.count(value) > 0) {
    return bool_literals_[value];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateBoolLiteral(value);
  op->SetType(type_context_->GetBooleanType());
  bool_literals_[value] = op;
  return op;
}

auto LLVMIRResourceContext::GetLongLiteral(std::int64_t value) -> middleend::llvm::utils::LongLiteral * {
  if (long_literals_.count(value) > 0) {
    return long_literals_[value];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateLongLiteral(value);
  op->SetType(type_context_->GetLongType());
  long_literals_[value] = op;
  return op;
}

auto LLVMIRResourceContext::GetCharLiteral(char value) -> middleend::llvm::utils::CharLiteral * {
  if (char_literals_.count(value) > 0) {
    return char_literals_[value];
  }
  // If the operand does not exist, create a new one
  auto op = middleend::llvm::utils::factory::OperandFactory::CreateCharLiteral(value);
  op->SetType(type_context_->GetCharType());
  char_literals_[value] = op;
  return op;
}

auto LLVMIRResourceContext::GetGlobalOperand(const std::string &name) -> middleend::llvm::utils::GlobalOperand * {
  if (global_operands_.count(name) > 0) {
    return global_operands_[name];
  }
  // If the operand does not exist, report error
  return nullptr;
}

// Function-related resource management
auto LLVMIRResourceContext::AddFunctionDef(const std::string &func_name,
                                           middleend::llvm::inst::FuncDefInst *func_def) -> void {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  module_->AddFunctionDef(func_name, func_def);
}

auto LLVMIRResourceContext::GetFunctionDef(const std::string &func_name) -> middleend::llvm::inst::FuncDefInst * {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  return module_->GetFunctionDef(func_name);
}

auto LLVMIRResourceContext::AddLibraryFunction(middleend::llvm::inst::FuncDeclareInst *func_def) -> void {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  module_->AddLibraryFuncDef(func_def);
}

auto LLVMIRResourceContext::GetLibraryFunction(const std::string &func_name)
    -> middleend::llvm::inst::FuncDeclareInst * {
  // First check if the function is a library function
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  for (const auto &lib_func : module_->GetLibraryFuncDefs()) {
    if (lib_func->GetFuncName() == func_name) {
      return lib_func;
    }
  }
  // If not found in library functions, check user-defined functions
  return nullptr;
}

auto LLVMIRResourceContext::AddFunction(middleend::llvm::utils::Function *function) -> void {
  // Check if the function already exists
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  for (const auto &func : module_->GetFunctions()) {
    if (func->GetFuncName() == function->GetFuncName()) {
      throw std::logic_error("Function already exists: " + function->GetFuncName());
    }
  }
  module_->AddFunction(function);
}

auto LLVMIRResourceContext::GetFunction(const std::string &func_name) -> middleend::llvm::utils::Function * {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  for (auto &func : module_->GetFunctions()) {
    if (func->GetFuncName() == func_name) {
      return func;
    }
  }
  // If the function does not exist, report error
  return nullptr;
}

/**
 * @brief Global variable-related resource management
 */
auto LLVMIRResourceContext::AddGlobalVarDef(const std::string &var_name,
                                            middleend::llvm::inst::GlobalDefInst *global_var_def) -> void {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  module_->AddGlobalVarDef(var_name, global_var_def);
}

auto LLVMIRResourceContext::GetGlobalVarDef(const std::string &var_name) -> middleend::llvm::inst::GlobalDefInst * {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  return module_->GetGlobalVarDef(var_name);
}

auto LLVMIRResourceContext::GetNewBasicBlock(middleend::llvm::utils::Function *function)
    -> middleend::llvm::utils::BasicBlock * {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  auto block = middleend::llvm::utils::factory::BlockFactory::CreateBlock(label_id_counter_++);
  function_blocks_mapping_[function].push_back(block);
  assert(block->GetID() == function_blocks_mapping_[function].size() - 1 &&
         "Block ID should be the index in the blocks vector");
  return block;
}

auto LLVMIRResourceContext::CreateFunction(const std::string &func_name,
                                           type::Type *return_type) -> middleend::llvm::utils::Function * {
  if (module_ == nullptr) {
    throw std::logic_error("Module is not set in LLVMIRContext");
  }
  auto function = middleend::llvm::utils::factory::FuncFactory::CreateFunction(func_name, return_type);
  AddFunction(function);
  return function;
}

}  // namespace context