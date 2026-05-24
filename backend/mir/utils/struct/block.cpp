#include "backend/mir/utils/struct/block.h"

namespace backend::utils {
Block::~Block() { instructions_.clear(); }

}  // namespace backend::utils