#ifndef BACKEND_TARGETS_UTILS_DIVMAGIC_H
#define BACKEND_TARGETS_UTILS_DIVMAGIC_H

#include <cstdint>

namespace backend::targetir {

/// Magic multiplier + shift for turning a signed division by a constant into a
/// high-multiply and shift (Hacker's Delight, fig. 10-1). Valid for any divisor
/// with |d| >= 2 and d != INT_MIN. The quotient of `n / d` is then:
///   q = mulhs(magic, n);                 // high 32 bits of the signed product
///   if (d > 0 && magic < 0) q += n;
///   if (d < 0 && magic > 0) q -= n;
///   q >>= shift;                         // arithmetic
///   q += (uint32_t)q >> 31;              // round toward zero
struct SignedDivMagic {
  int32_t magic_;
  int shift_;
};

inline auto ComputeSignedDivMagic(int32_t d) -> SignedDivMagic {
  const uint32_t two31 = 0x80000000U;
  uint32_t ad = d < 0 ? 0U - static_cast<uint32_t>(d) : static_cast<uint32_t>(d);
  uint32_t t = two31 + (static_cast<uint32_t>(d) >> 31);
  uint32_t anc = t - 1 - t % ad;  // |nc|, the largest n with (n % d) == d - 1
  int p = 31;
  uint32_t q1 = two31 / anc;
  uint32_t r1 = two31 - q1 * anc;
  uint32_t q2 = two31 / ad;
  uint32_t r2 = two31 - q2 * ad;
  uint32_t delta = 0;
  do {
    p = p + 1;
    q1 = 2 * q1;
    r1 = 2 * r1;
    if (r1 >= anc) {
      q1 = q1 + 1;
      r1 = r1 - anc;
    }
    q2 = 2 * q2;
    r2 = 2 * r2;
    if (r2 >= ad) {
      q2 = q2 + 1;
      r2 = r2 - ad;
    }
    delta = ad - r2;
  } while (q1 < delta || (q1 == delta && r1 == 0));

  SignedDivMagic result;
  result.magic_ = static_cast<int32_t>(q2 + 1);
  if (d < 0) {
    result.magic_ = -result.magic_;
  }
  result.shift_ = p - 32;
  return result;
}

}  // namespace backend::targetir

#endif
