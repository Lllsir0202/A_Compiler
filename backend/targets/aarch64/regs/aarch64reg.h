#ifndef BACKEND_TARGETS_AARCH64_REGS_AARCH64REG_H
#define BACKEND_TARGETS_AARCH64_REGS_AARCH64REG_H

#include <array>
#include <cstdint>

namespace backend::targetir::aarch64 {

/**
 * @brief AArch64 (ARMv8-A, AAPCS64/LP64) physical register numbering.
 *
 * GPR ids 0..30 are x0..x30 (the 32-bit views are w0..w30); id 31 is the stack
 * pointer sp. The 32/64-bit width is carried by the operand's register *size*
 * (the printer renders w vs x, s vs d) rather than by the mnemonic — this is the
 * key difference from RISC-V, where the width lives in the mnemonic (addw vs add)
 * and the register name is size-independent.
 *
 * FPR ids 0..31 are v0..v31; SysY single-precision floats use the s-view (s0..s31,
 * the low 32 bits of each v register).
 */
enum class GPR : uint8_t {
  X0 = 0, X1, X2, X3, X4, X5, X6, X7,      // arg / return (caller-saved)
  X8,                                      // indirect result location (caller-saved)
  X9, X10, X11, X12, X13, X14, X15,        // temporaries (caller-saved)
  X16, X17,                                // IP0/IP1 (caller-saved; reserved: veneers + spill scratch)
  X18,                                     // platform register (reserved)
  X19, X20, X21, X22, X23, X24, X25, X26, X27, X28,  // callee-saved
  X29,                                     // fp (frame pointer, callee-saved, reserved)
  X30,                                     // lr (link register, reserved)
  Sp = 31,                                 // stack pointer
  Fp = 29,
  Lr = 30,
};

enum class FPR : uint8_t {
  V0 = 0, V1, V2, V3, V4, V5, V6, V7,      // arg / return (caller-saved)
  V8, V9, V10, V11, V12, V13, V14, V15,    // callee-saved (only low 64 bits d8..d15)
  V16, V17, V18, V19, V20, V21, V22, V23,  // caller-saved
  V24, V25, V26, V27, V28, V29, V30, V31,
};

// Integer argument / return registers (x0..x7 / w0..w7).
inline constexpr std::array<GPR, 8> INT_ARG_REGS = {GPR::X0, GPR::X1, GPR::X2, GPR::X3,
                                                    GPR::X4, GPR::X5, GPR::X6, GPR::X7};
// Floating argument / return registers (v0..v7 / s0..s7).
inline constexpr std::array<FPR, 8> FLOAT_ARG_REGS = {FPR::V0, FPR::V1, FPR::V2, FPR::V3,
                                                      FPR::V4, FPR::V5, FPR::V6, FPR::V7};

/// Allocatable GPRs. Excludes x16 (spill/frame address scratch), x17/x18
/// (IP1/platform, reserved), x29 (fp), x30 (lr) and sp.
inline constexpr std::array<GPR, 26> ALLOCATABLE_GPR = {
    GPR::X0,  GPR::X1,  GPR::X2,  GPR::X3,  GPR::X4,  GPR::X5,  GPR::X6,  GPR::X7,  GPR::X8,
    GPR::X9,  GPR::X10, GPR::X11, GPR::X12, GPR::X13, GPR::X14, GPR::X15, GPR::X19, GPR::X20,
    GPR::X21, GPR::X22, GPR::X23, GPR::X24, GPR::X25, GPR::X26, GPR::X27, GPR::X28};

/// Allocatable FPRs (all of v0..v31).
inline constexpr std::array<FPR, 32> ALLOCATABLE_FPR = {
    FPR::V0,  FPR::V1,  FPR::V2,  FPR::V3,  FPR::V4,  FPR::V5,  FPR::V6,  FPR::V7,
    FPR::V8,  FPR::V9,  FPR::V10, FPR::V11, FPR::V12, FPR::V13, FPR::V14, FPR::V15,
    FPR::V16, FPR::V17, FPR::V18, FPR::V19, FPR::V20, FPR::V21, FPR::V22, FPR::V23,
    FPR::V24, FPR::V25, FPR::V26, FPR::V27, FPR::V28, FPR::V29, FPR::V30, FPR::V31};

/// True if a GPR id is caller-saved (x0..x18 and lr) and so clobbered across a call.
inline auto IsGPRCallerSaved(uint8_t id) -> bool {
  return id <= static_cast<uint8_t>(GPR::X18) || id == static_cast<uint8_t>(GPR::Lr);
}

/// True if an FPR id is caller-saved (v0..v7, v16..v31); v8..v15 are callee-saved.
inline auto IsFPRCallerSaved(uint8_t id) -> bool {
  return id <= static_cast<uint8_t>(FPR::V7) || id >= static_cast<uint8_t>(FPR::V16);
}

}  // namespace backend::targetir::aarch64

#endif
