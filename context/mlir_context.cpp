#include "context/mlir_context.h"
#include <stdexcept>
#include "middleend/mlir/dialect/func/funcdef.h"
#include "middleend/mlir/utils/value/value.h"

namespace context {
auto MLIRContext::RegisterLibraryFunction() -> void {
  // Register the library function
  // note that this function should be called after module_op created
  /**
   * FuncDefOp Create has following args:
   * @param context: the MLIR context -> this
   * @param name: the name of the function
   * @param args: the arguments of the function, which are values
   * @param return_type: the return type of the function
   */
  auto int_type = type_context_->GetIntegerType();
  auto float_type = type_context_->GetFloatType();
  auto void_type = type_context_->GetVoidType();
  auto int_ptr_type = type_context_->GetPtrType(int_type);
  auto float_ptr_type = type_context_->GetPtrType(float_type);

  // int getint();
  auto getint_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "getint", std::vector<middleend::mlir::Value *>{}, int_type);

  // int getch();
  auto getch_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "getch", std::vector<middleend::mlir::Value *>{}, int_type);

  // float getfloat();
  auto getfloat_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "getfloat", std::vector<middleend::mlir::Value *>{}, float_type);

  // int getarray(int[]);
  auto getarray_val = CreateValue();

  getarray_val->SetType(int_ptr_type);
  auto getarray_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "getarray", std::vector<middleend::mlir::Value *>{getarray_val}, int_type);

  // int getfarray(float[]);
  auto getfarray_val = CreateValue();
  getfarray_val->SetType(float_ptr_type);
  auto getfarray_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "getfarray", std::vector<middleend::mlir::Value *>{getfarray_val}, int_type);

  // void putint(int);
  auto putint_val = CreateValue();
  putint_val->SetType(int_type);
  auto putint_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "putint", std::vector<middleend::mlir::Value *>{putint_val}, void_type);

  // void putch(int);
  auto putch_val = CreateValue();
  putch_val->SetType(int_type);
  auto putch_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "putch", std::vector<middleend::mlir::Value *>{putch_val}, void_type);

  // void putfloat(float);
  auto putfloat_val = CreateValue();
  putfloat_val->SetType(float_type);
  auto putfloat_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "putfloat", std::vector<middleend::mlir::Value *>{putfloat_val}, void_type);

  // void putarray(int, int[]);
  auto putarray_size_val = CreateValue();
  putarray_size_val->SetType(int_type);
  auto putarray_arr_val = CreateValue();
  putarray_arr_val->SetType(int_ptr_type);
  auto putarray_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "putarray", std::vector<middleend::mlir::Value *>{putarray_size_val, putarray_arr_val}, void_type);

  // void putfarray(int, float[]);
  auto putfarray_size_val = CreateValue();
  putfarray_size_val->SetType(int_type);
  auto putfarray_arr_val = CreateValue();
  putfarray_arr_val->SetType(float_ptr_type);
  auto putfarray_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "putfarray", std::vector<middleend::mlir::Value *>{putfarray_size_val, putfarray_arr_val}, void_type);

  // void putf(char[], int);
  // Not deal with now

  // void starttime();
  auto starttime_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "starttime", std::vector<middleend::mlir::Value *>{}, void_type);

  // void stoptime();
  auto stoptime_func_def_op = CreateOperation<middleend::mlir::func::FuncDefOp>(
      this, "stoptime", std::vector<middleend::mlir::Value *>{}, void_type);

  // Add the function definition operations to the module operation
  if (module_op_ == nullptr || module_op_->regions_.empty() || module_op_->regions_[0]->blocks_.empty()) {
    throw std::logic_error("Module operation or its region/block is not properly initialized.");
  }
  module_op_->regions_[0]->blocks_[0]->AddOperation(getint_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(getch_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(getfloat_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(getarray_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(getfarray_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(putint_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(putch_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(putfloat_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(putarray_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(putfarray_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(starttime_func_def_op);
  module_op_->regions_[0]->blocks_[0]->AddOperation(stoptime_func_def_op);

  // Map function name to function definition operation
  resource_context_->MapFuncToFuncDefOp("getint", getint_func_def_op);
  resource_context_->MapFuncToFuncDefOp("getch", getch_func_def_op);
  resource_context_->MapFuncToFuncDefOp("getfloat", getfloat_func_def_op);
  resource_context_->MapFuncToFuncDefOp("getarray", getarray_func_def_op);
  resource_context_->MapFuncToFuncDefOp("getfarray", getfarray_func_def_op);
  resource_context_->MapFuncToFuncDefOp("putint", putint_func_def_op);
  resource_context_->MapFuncToFuncDefOp("putch", putch_func_def_op);
  resource_context_->MapFuncToFuncDefOp("putfloat", putfloat_func_def_op);
  resource_context_->MapFuncToFuncDefOp("putarray", putarray_func_def_op);
  resource_context_->MapFuncToFuncDefOp("putfarray", putfarray_func_def_op);
  resource_context_->MapFuncToFuncDefOp("starttime", starttime_func_def_op);
  resource_context_->MapFuncToFuncDefOp("stoptime", stoptime_func_def_op);
}

}  // namespace context