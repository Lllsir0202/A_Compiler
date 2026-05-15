#ifndef MIDDLEEND_PASS_TRANSFORM_TRANSFORM_H
#define MIDDLEEND_PASS_TRANSFORM_TRANSFORM_H
#include "middleend/pass/pass.h"
namespace middleend::pass::transform {
class TransformPass : public Pass {
  using Pass::Pass;

 public:
  ~TransformPass() override = default;

  auto IsAnalysisPass() -> bool override { return false; }

  // Transform pass will not be invalid
  auto IsValid() -> bool override { return true; }
};

}  // namespace middleend::pass::transform

#endif