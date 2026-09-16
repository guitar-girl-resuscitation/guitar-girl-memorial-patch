#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace ggfm {
// The stock BigInteger suffix list has 702 entries (A..ZZ). Preserve normal
// formatting; beyond its range emit a bounded scientific preview, never index
// the fixed suffix array or allocate a decimal string proportional to the value.
inline std::string LargeNumberPreview(const std::uint32_t* limbs, std::size_t count,
                                      bool negative, int precision) {
  if (!limbs || count < 218) return {};
  // BigInteger limbs are normalized, little endian. Read only the top two.
  const double top = static_cast<double>(limbs[count - 1]) * 4294967296.0 + limbs[count - 2];
  if (top == 0.0) return {};
  const double logarithm = std::log10(top) + static_cast<double>(count - 2) * 9.632959861247398;
  // A small safety margin avoids a floating-point estimate dispatching an
  // out-of-range boundary value back into the stock array lookup.
  if (logarithm < 2109.0 - 1e-9) return {};
  auto exponent = static_cast<std::uint64_t>(std::floor(logarithm));
  double mantissa = std::pow(10.0, logarithm - static_cast<double>(exponent));
  precision = precision < 0 ? 0 : (precision > 6 ? 6 : precision);
  const double scale = std::pow(10.0, precision);
  if (std::round(mantissa * scale) >= 10.0 * scale) { mantissa = 1.0; ++exponent; }
  char buffer[80];
  std::snprintf(buffer, sizeof(buffer), "%s%.*fe%llu", negative ? "-" : "", precision,
                mantissa, static_cast<unsigned long long>(exponent));
  return buffer;
}
}
