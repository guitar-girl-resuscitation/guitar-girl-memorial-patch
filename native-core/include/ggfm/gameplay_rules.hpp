#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

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

// A one-line preview that does not fit its label - a chat room's latest message,
// a follower profile line, a long toast - is clamped and ends with this shipped
// suffix. The labels drawing those previews have rich text off, so all ten
// characters print literally, and "[ff]" is not valid NGUI markup either (colour
// tags carry six hex digits), so even a label with encoding on would print it.
// The player sees a line break into "[-]" and "[ff]..." where the sentence
// should simply trail off.
constexpr std::u16string_view kClampedPreviewSuffix = u"[-][ff]...";
constexpr std::u16string_view kPreviewEllipsis = u"...";

// Where the suffix begins, or npos when the text does not end in one. The label
// wraps its own output, so the suffix can arrive broken across lines - on screen
// that reads as a row ending in "[-]" with "[ff]..." underneath - and line breaks
// inside or after it are skipped rather than failing the match.
constexpr std::size_t ClampedPreviewSuffixStart(std::u16string_view processed) {
  std::size_t remaining = kClampedPreviewSuffix.size();
  std::size_t index = processed.size();
  while (index > 0 && remaining > 0) {
    const char16_t character = processed[index - 1];
    --index;
    if (character == u'\n') continue;
    if (character != kClampedPreviewSuffix[remaining - 1]) return std::u16string_view::npos;
    --remaining;
  }
  return remaining == 0 ? index : std::u16string_view::npos;
}

constexpr bool EndsWithClampedPreviewSuffix(std::u16string_view processed) {
  return ClampedPreviewSuffixStart(processed) != std::u16string_view::npos;
}

// Keeps the ellipsis the suffix was meant to show, drops the markup around it.
// Returns the text unchanged when it does not end in the suffix. The wrap the
// suffix needed goes with it, so a repaired preview fits the line it started on.
inline std::u16string RepairClampedPreview(std::u16string_view processed) {
  const auto start = ClampedPreviewSuffixStart(processed);
  if (start == std::u16string_view::npos) return std::u16string(processed);
  std::u16string repaired(processed.substr(0, start));
  while (!repaired.empty() && repaired.back() == u'\n') repaired.pop_back();
  repaired.append(kPreviewEllipsis);
  return repaired;
}
}  // namespace ggfm
