#pragma once

#include "ggfm/hook_backend.hpp"
#include <array>
#include <cstdint>
#include <string_view>

namespace ggfm {
// Selectively adapted from PR #2 (519489b): cache the stock result, not a
// replacement formula. No managed references, UI rows or unbounded maps.
class FanMultiplierMemo {
 public:
  bool Find(std::uintptr_t owner, std::int32_t area, std::int32_t level,
            std::uint64_t revision, double& value) const {
    if (area < 1 || area > 2) return false;
    const auto& e = entries_[static_cast<std::size_t>(area - 1)];
    if (!e.valid || e.owner != owner || e.level != level || e.revision != revision) return false;
    value = e.value;
    return true;
  }
  void Store(std::uintptr_t owner, std::int32_t area, std::int32_t level,
             std::uint64_t revision, double value) {
    if (area < 1 || area > 2) return;
    entries_[static_cast<std::size_t>(area - 1)] = {owner, level, revision, value, true};
  }
 private:
  struct Entry {
    std::uintptr_t owner{};
    std::int32_t level{};
    std::uint64_t revision{};
    double value{};
    bool valid{};
  };
  std::array<Entry, 2> entries_{};
};

bool InitializeFanMultiplierCache(std::uintptr_t il2cpp_base);
HookBinding ResolveFanMultiplierCacheHook(std::string_view name);
}  // namespace ggfm
