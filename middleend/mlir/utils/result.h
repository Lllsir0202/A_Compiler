#ifndef MIDDLEEND_MLIR_UTILS_RESULT_H
#define MIDDLEEND_MLIR_UTILS_RESULT_H

namespace middleend::mlir {

class Result {
 private:
  enum class ResultType { Success, Failure };

 public:
  explicit Result(ResultType type) : type_(type) {}
  // Used to create specific results
  static auto Success() -> Result { return Result(ResultType::Success); }
  static auto Failure() -> Result { return Result(ResultType::Failure); }

  auto IsSuccess() const -> bool { return type_ == ResultType::Success; }

 private:
  ResultType type_;
};

}  // namespace middleend::mlir

#endif