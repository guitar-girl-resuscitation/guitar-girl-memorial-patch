#include "ggfm/fan_multiplier_cache.hpp"
#include "ggfm/gameplay_rules.hpp"

namespace ggfm {
namespace {
#include "ggfm/generated_hook_targets.inc"

// UserInfo's fan multiplier (0x202C298) and fan level (0x202C320): both take the
// UserInfo table and an area, the multiplier returning a double.
using FanMultiplier = double (*)(void*, std::int32_t, const void*);
using FanLevel = std::int32_t (*)(void*, std::int32_t, const void*);
FanMultiplier original_fan_multiplier = nullptr;
FanLevel fan_level = nullptr;
bool declared = false;

// Unity's main thread only.
FanMultiplierMemo memo;

double FanMultiplierHook(void* user_info, const std::int32_t area, const void* method) {
  const auto level = fan_level(user_info, area, nullptr);
  double multiplier = 0;
  if (memo.Find(area, level, multiplier)) return multiplier;
  multiplier = original_fan_multiplier(user_info, area, method);
  memo.Store(area, level, multiplier);
  return multiplier;
}
}  // namespace

bool InitializeFanMultiplierCache(const std::uintptr_t il2cpp_base) {
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == "gameplay.fan.level")
      fan_level = reinterpret_cast<FanLevel>(il2cpp_base + dependency.rva);
  }
  for (const auto& target : kGeneratedHookTargets) {
    if (target.name == "gameplay.fan.multiplier") declared = true;
  }
  return !declared || fan_level != nullptr;
}

HookBinding ResolveFanMultiplierCacheHook(const std::string_view name) {
  if (name == "gameplay.fan.multiplier" && fan_level != nullptr)
    return {reinterpret_cast<void*>(FanMultiplierHook),
            reinterpret_cast<void**>(&original_fan_multiplier)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
