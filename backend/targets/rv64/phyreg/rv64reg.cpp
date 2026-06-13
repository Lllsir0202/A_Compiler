#include "backend/targets/rv64/phyreg/rv64reg.h"

namespace backend::targetir {
auto RV64RegPrinter::GetRegName([[maybe_unused]] TargetRegSize reg_size, uint8_t id, TargetRegClass reg_class,
                                TargetRegRole role) const -> std::string {}

}  // namespace backend::targetir