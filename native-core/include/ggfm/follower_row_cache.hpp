#pragma once

#include "ggfm/hook_backend.hpp"

#include <cstdint>
#include <string_view>

namespace ggfm {
// Keeps a follower row's Likes/sec label across taps; see RecomputeRowLabel.
HookBinding ResolveFollowerRowCacheHook(std::string_view name);

// A successful level-up changes the figures the label shows.
void NotifyFollowerLevelChanged(std::int32_t follower_id);
void NotifyCharacterLevelChanged();
}  // namespace ggfm
