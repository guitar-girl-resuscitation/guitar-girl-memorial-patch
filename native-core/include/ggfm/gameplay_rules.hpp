#pragma once

#include <array>
#include <cstdint>

namespace ggfm {
// Only the active, fully initialized table graph may bind a late snapshot.
// During cold-start the stock loader must keep deferring until tables exist.
constexpr bool RefreshLoadedContent(bool requested, bool tables_initialized,
                                    bool active_manager) {
  return requested || (tables_initialized && active_manager);
}

constexpr std::int64_t LocalDayEndTicks(std::int64_t unix_seconds,
                                       std::int32_t utc_offset_minutes) {
  const std::int64_t local_seconds = unix_seconds + utc_offset_minutes * 60LL;
  const std::int64_t day = local_seconds / 86400 -
      (local_seconds < 0 && local_seconds % 86400 != 0 ? 1 : 0);
  return ((day + 1) * 86400 - 1 + 62135596800LL) * 10000000LL;
}

struct QuestClaimProjection {
  std::array<bool, 3> received{};
  std::int64_t usn = 0;

  void Update(std::int64_t owner, const std::array<std::int16_t, 3>& flags) {
    usn = owner;
    for (std::size_t i = 0; i < received.size(); ++i) received[i] = flags[i] == 1;
  }

  int State(std::int64_t owner, int index, int stock_state) const {
    return owner == usn && index >= 1 && index <= 3 && received[index - 1]
        ? 2 : stock_state;
  }
};
}  // namespace ggfm
