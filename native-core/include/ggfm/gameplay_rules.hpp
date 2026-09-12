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

// The stock RPC clock represents Unix time as UTC+9 DateTime ticks, independent
// of device timezone. Keep rotation at device midnight, but express BOTH bounds
// in the clock used by SubscribeList.IsActive and the countdown UI.
constexpr std::int64_t PassDayEndTicks(std::int64_t unix_seconds,
                                      std::int32_t utc_offset_minutes) {
  return LocalDayEndTicks(unix_seconds, utc_offset_minutes) +
      (540LL - utc_offset_minutes) * 60 * 10000000LL;
}

constexpr std::int64_t PassDayStartTicks(std::int64_t unix_seconds,
                                        std::int32_t utc_offset_minutes) {
  return PassDayEndTicks(unix_seconds, utc_offset_minutes) - 86399LL * 10000000LL;
}

// --- Follower row "Likes/sec" label ------------------------------------------
// Every visible follower row re-derives its Likes/sec figure on each tap, skill tap
// and currency change (GgUIFollowerItemController subscribes its whole refresh to
// the core tap event). That one label costs ~19 ms per row on a Lv. 1000 roster
// under ARM translation -- the rest of the refresh is ~0.2 ms -- so four or five
// visible rows took taps, bulk level-ups and bulk claims down to ~10 fps. Nothing
// a tap changes feeds that figure. The row keeps the text it already shows unless
// its follower changed, a level-up happened, or the text is older than
// kRowLabelMaxAgeNs. An expired-only row also waits until kRowLabelFrameNs after
// the END of the last recompute: all visible rows refresh inside one event
// dispatch and each recompute outlasts the gap, so stamping the start would let
// every expired row through at once.
constexpr std::int64_t kRowLabelMaxAgeNs = 3'000'000'000;
constexpr std::int64_t kRowLabelFrameNs = 12'000'000;

struct RowLabelStamp {
  std::int32_t follower_id;
  std::uint64_t follower_generation;
  std::uint64_t global_generation;
  std::int64_t computed_ns;
};

constexpr bool RecomputeRowLabel(bool known, const RowLabelStamp& seen,
                                 std::int32_t follower_id,
                                 std::uint64_t follower_generation,
                                 std::uint64_t global_generation, std::int64_t now_ns,
                                 std::int64_t last_recompute_end_ns) {
  if (!known || seen.follower_id != follower_id ||
      seen.follower_generation != follower_generation ||
      seen.global_generation != global_generation)
    return true;
  if (now_ns - seen.computed_ns < kRowLabelMaxAgeNs) return false;
  return now_ns - last_recompute_end_ns >= kRowLabelFrameNs;
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
