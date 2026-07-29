#ifndef BACKEND_TARGETS_RISCV_REGS_RISCVREG_H
#define BACKEND_TARGETS_RISCV_REGS_RISCVREG_H

#include <array>
#include <cstdint>
#include <string>
#include "backend/targets/utils/operand/targetoperand.h"

namespace backend::targetir::riscv {

/**
 * @brief RISC-V (rv64) physical register numbering.
 *
 * We use the raw register index (0..31) as the id carried by TargetPhysicalReg,
 * separately for the integer (GPR, x0..x31) and floating (FPR, f0..f31) files.
 * The register class stored on the operand decides which file an id refers to,
 * so the same numeric id is reused across the two files without ambiguity.
 */
enum class GPR : uint8_t {
  Zero = 0,  // x0  hard-wired zero
  Ra = 1,    // x1  return address           (callee-saved by convention: saved in prologue)
  Sp = 2,    // x2  stack pointer
  Gp = 3,    // x3  global pointer            (reserved)
  Tp = 4,    // x4  thread pointer            (reserved)
  T0 = 5,    // x5  temporary                 (caller-saved)
  T1 = 6,    // x6
  T2 = 7,    // x7
  Fp = 8,    // x8  s0 / frame pointer        (callee-saved)
  S1 = 9,    // x9                            (callee-saved)
  A0 = 10,   // x10 arg / return value        (caller-saved)
  A1 = 11,   // x11 arg / return value
  A2 = 12,   // x12 arg
  A3 = 13,   // x13
  A4 = 14,   // x14
  A5 = 15,   // x15
  A6 = 16,   // x16
  A7 = 17,   // x17
  S2 = 18,   // x18                           (callee-saved)
  S3 = 19,
  S4 = 20,
  S5 = 21,
  S6 = 22,
  S7 = 23,
  S8 = 24,
  S9 = 25,
  S10 = 26,
  S11 = 27,
  T3 = 28,  // x28 temporary                  (caller-saved)
  T4 = 29,
  T5 = 30,
  T6 = 31,
};

enum class FPR : uint8_t {
  Ft0 = 0,  // f0  temporary                 (caller-saved)
  Ft1 = 1,
  Ft2 = 2,
  Ft3 = 3,
  Ft4 = 4,
  Ft5 = 5,
  Ft6 = 6,
  Ft7 = 7,
  Fs0 = 8,  // f8                            (callee-saved)
  Fs1 = 9,
  Fa0 = 10,  // f10 arg / return value        (caller-saved)
  Fa1 = 11,
  Fa2 = 12,
  Fa3 = 13,
  Fa4 = 14,
  Fa5 = 15,
  Fa6 = 16,
  Fa7 = 17,
  Fs2 = 18,  // f18                           (callee-saved)
  Fs3 = 19,
  Fs4 = 20,
  Fs5 = 21,
  Fs6 = 22,
  Fs7 = 23,
  Fs8 = 24,
  Fs9 = 25,
  Fs10 = 26,
  Fs11 = 27,
  Ft8 = 28,  // f28 temporary                 (caller-saved)
  Ft9 = 29,
  Ft10 = 30,
  Ft11 = 31,
};

// ABI names, indexed by raw register id, used by the printer.
inline constexpr std::array<const char *, 32> GPR_ABI_NAME = {
    "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0",  "a1",  "a2", "a3", "a4", "a5",
    "a6",   "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};

inline constexpr std::array<const char *, 32> FPR_ABI_NAME = {
    "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7", "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
    "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7", "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

// Integer argument / return registers (a0..a7).
inline constexpr std::array<GPR, 8> INT_ARG_REGS = {GPR::A0, GPR::A1, GPR::A2, GPR::A3,
                                                    GPR::A4, GPR::A5, GPR::A6, GPR::A7};
// Floating argument / return registers (fa0..fa7).
inline constexpr std::array<FPR, 8> FLOAT_ARG_REGS = {FPR::Fa0, FPR::Fa1, FPR::Fa2, FPR::Fa3,
                                                      FPR::Fa4, FPR::Fa5, FPR::Fa6, FPR::Fa7};

/// Allocatable GPRs for the register allocator. Excludes zero/ra/sp/gp/tp (fixed
/// by the ABI), fp (reserved as the frame pointer by the frame layer), and t3
/// (reserved as the spill/frame address scratch for offsets outside the ±2047
/// immediate range).
inline constexpr std::array<GPR, 25> ALLOCATABLE_GPR = {GPR::T0,  GPR::T1, GPR::T2, GPR::S1, GPR::A0, GPR::A1, GPR::A2,
                                                        GPR::A3,  GPR::A4, GPR::A5, GPR::A6, GPR::A7, GPR::S2, GPR::S3,
                                                        GPR::S4,  GPR::S5, GPR::S6, GPR::S7, GPR::S8, GPR::S9, GPR::S10,
                                                        GPR::S11, GPR::T4, GPR::T5, GPR::T6};

/// Allocatable FPRs.
inline constexpr std::array<FPR, 32> ALLOCATABLE_FPR = {
    FPR::Ft0, FPR::Ft1, FPR::Ft2, FPR::Ft3, FPR::Ft4,  FPR::Ft5,  FPR::Ft6, FPR::Ft7, FPR::Fs0,  FPR::Fs1, FPR::Fa0,
    FPR::Fa1, FPR::Fa2, FPR::Fa3, FPR::Fa4, FPR::Fa5,  FPR::Fa6,  FPR::Fa7, FPR::Fs2, FPR::Fs3,  FPR::Fs4, FPR::Fs5,
    FPR::Fs6, FPR::Fs7, FPR::Fs8, FPR::Fs9, FPR::Fs10, FPR::Fs11, FPR::Ft8, FPR::Ft9, FPR::Ft10, FPR::Ft11};

/// True if a GPR id is caller-saved (t*, a*) and therefore clobbered across a call.
inline auto IsGPRCallerSaved(uint8_t id) -> bool {
  return (id >= static_cast<uint8_t>(GPR::T0) && id <= static_cast<uint8_t>(GPR::T2)) ||
         (id >= static_cast<uint8_t>(GPR::A0) && id <= static_cast<uint8_t>(GPR::A7)) ||
         (id >= static_cast<uint8_t>(GPR::T3) && id <= static_cast<uint8_t>(GPR::T6));
}

/// True if an FPR id is caller-saved (ft*, fa*).
inline auto IsFPRCallerSaved(uint8_t id) -> bool {
  return (id >= static_cast<uint8_t>(FPR::Ft0) && id <= static_cast<uint8_t>(FPR::Ft7)) ||
         (id >= static_cast<uint8_t>(FPR::Fa0) && id <= static_cast<uint8_t>(FPR::Fa7)) ||
         (id >= static_cast<uint8_t>(FPR::Ft8) && id <= static_cast<uint8_t>(FPR::Ft11));
}

}  // namespace backend::targetir::riscv

#endif
