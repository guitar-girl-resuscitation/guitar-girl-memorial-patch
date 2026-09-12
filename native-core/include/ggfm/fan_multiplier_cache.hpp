#pragma once

#include "ggfm/hook_backend.hpp"

#include <cstdint>
#include <string_view>

namespace ggfm {
// Remembers the per-area fan multiplier for the fan level it was built at; see
// FanMultiplierMemo. Returns false only when the manifest declares the multiplier
// hook but not the fan-level lookup it depends on.
bool InitializeFanMultiplierCache(std::uintptr_t il2cpp_base);

HookBinding ResolveFanMultiplierCacheHook(std::string_view name);
}  // namespace ggfm
