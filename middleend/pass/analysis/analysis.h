#ifndef MIDDLEEND_PASS_ANALYSIS_ANALYSIS_H
#define MIDDLEEND_PASS_ANALYSIS_ANALYSIS_H
#include "middleend/pass/pass.h"
namespace middleend::pass::analysis {
class AnalysisPass : public Pass {
  // Override the IsAnalysisPass function to return true,
  // so that we can verify a pass easier
  using Pass::Pass;

 public:
  ~AnalysisPass() override = default;
  auto IsAnalysisPass() -> bool override { return true; }
};

}  // namespace middleend::pass::analysis

#endif