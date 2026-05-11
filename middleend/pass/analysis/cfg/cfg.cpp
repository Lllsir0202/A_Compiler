#include "middleend/pass/analysis/cfg/cfg.h"

namespace middleend::pass::analysis {
CFGNode::~CFGNode() = default;
FunctionCFG::~FunctionCFG() = default;
CFGBuilder::~CFGBuilder() = default;
}  // namespace middleend::pass::analysis