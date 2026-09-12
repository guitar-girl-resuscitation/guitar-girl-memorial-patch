#include "ggfm/follower_row_cache.hpp"
#include "ggfm/gameplay_rules.hpp"

#include <time.h>
#include <unordered_map>

namespace ggfm {
namespace {
// Both row classes (GgUIFollowerItemController for Channel 1, _2 for Channel 2) keep
// the follower id at +0x198 (written by SetData) and pass it to the stock production
// calculation 0x22D265C. The label step's only effect is UILabel.set_text on +0x78,
// so skipping it leaves the text already shown.
constexpr std::uintptr_t kFollowerIdOffset = 0x198;

using RowMethod = void (*)(void*, const void*);
RowMethod original_label_ch1 = nullptr;
RowMethod original_label_ch2 = nullptr;
RowMethod original_enable_ch1 = nullptr;
RowMethod original_enable_ch2 = nullptr;

// Unity's main thread only: rows, taps and level-ups all run there.
std::unordered_map<void*, RowLabelStamp> rows;
std::unordered_map<std::int32_t, std::uint64_t> follower_generations;
std::uint64_t global_generation = 0;
std::int64_t last_recompute_end_ns = 0;

std::int64_t NowNs() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

void LikesLabel(const RowMethod original, void* row, const void* method) {
  if (row == nullptr) return original(row, method);
  const auto follower_id =
      *reinterpret_cast<const std::int32_t*>(static_cast<char*>(row) + kFollowerIdOffset);
  const auto generation = follower_generations.find(follower_id);
  const std::uint64_t follower_generation =
      generation == follower_generations.end() ? 0 : generation->second;
  const auto now = NowNs();
  const auto seen = rows.find(row);
  const bool known = seen != rows.end();
  const RowLabelStamp stamp = known ? seen->second : RowLabelStamp{};
  if (!RecomputeRowLabel(known, stamp, follower_id, follower_generation, global_generation,
                         now, last_recompute_end_ns))
    return;
  original(row, method);
  // Stamp the end: any recompute (change-driven ones too) holds expired rows back.
  last_recompute_end_ns = NowNs();
  rows[row] = {follower_id, follower_generation, global_generation, now};
}

void LikesLabelCh1(void* row, const void* method) { LikesLabel(original_label_ch1, row, method); }
void LikesLabelCh2(void* row, const void* method) { LikesLabel(original_label_ch2, row, method); }

// OnEnable: a re-enabled or newly built row (possibly at a reused address) draws fresh.
void EnableCh1(void* row, const void* method) {
  rows.erase(row);
  original_enable_ch1(row, method);
}
void EnableCh2(void* row, const void* method) {
  rows.erase(row);
  original_enable_ch2(row, method);
}
}  // namespace

void NotifyFollowerLevelChanged(const std::int32_t follower_id) {
  ++follower_generations[follower_id];
}
void NotifyCharacterLevelChanged() { ++global_generation; }

HookBinding ResolveFollowerRowCacheHook(const std::string_view name) {
  if (name == "gameplay.followerRow.likesLabel")
    return {reinterpret_cast<void*>(LikesLabelCh1), reinterpret_cast<void**>(&original_label_ch1)};
  if (name == "gameplay.followerRow.enable")
    return {reinterpret_cast<void*>(EnableCh1), reinterpret_cast<void**>(&original_enable_ch1)};
  if (name == "gameplay.followerRow2.likesLabel")
    return {reinterpret_cast<void*>(LikesLabelCh2), reinterpret_cast<void**>(&original_label_ch2)};
  if (name == "gameplay.followerRow2.enable")
    return {reinterpret_cast<void*>(EnableCh2), reinterpret_cast<void**>(&original_enable_ch2)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
