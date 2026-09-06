#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ggfm {
// IL2CPP's ObscuredFloat is a 20-byte, 4-aligned value on both supported ABIs.
// Let the compiler supply the native struct-return ABI; do not hand-code sret.
struct UpgradeEncodedFloat { std::uint32_t words[5]; };
static_assert(sizeof(UpgradeEncodedFloat) == 20 && alignof(UpgradeEncodedFloat) == 4);
using EncodeUpgradeFloat = UpgradeEncodedFloat (*)(float, const void*);

inline bool RestoreUpgradeIncrement(void* row, const void* dto,
                                    std::size_t source_offset, std::size_t target_offset,
                                    EncodeUpgradeFloat encode) {
  if (!row || !dto || !encode) return false;
  double source = 0;
  std::memcpy(&source, static_cast<const char*>(dto) + source_offset, sizeof(source));
  const float increment = static_cast<float>(source);
  if (!std::isfinite(increment) || increment < 0) return false;
  const auto encoded = encode(increment, nullptr);
  std::memcpy(static_cast<char*>(row) + target_offset, &encoded, sizeof(encoded));
  return true;
}
}  // namespace ggfm
