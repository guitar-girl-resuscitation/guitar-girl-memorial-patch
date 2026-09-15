#include "ggfm/fan_multiplier_cache.hpp"
#include <atomic>

namespace ggfm {
namespace {
#include "ggfm/generated_hook_targets.inc"
using Multiplier = double (*)(void*, std::int32_t, const void*);
using Level = std::int32_t (*)(void*, std::int32_t, const void*);
using Clear = void (*)(void*, const void*);
using SetRow = void (*)(void*, std::int32_t, void*, const void*);
using LoadRpc = bool (*)(void*, void*, const void*);
// IL2CPP System.Nullable<bool>, same two-byte value type on both target ABIs.
struct NullableBool { bool has_value; bool value; };
static_assert(sizeof(NullableBool) == 2);
using LoadBytes = NullableBool (*)(void*, void*, const void*);
Multiplier original_multiplier = nullptr;
Level fan_level = nullptr;
Clear original_clear = nullptr;
SetRow original_set_row = nullptr;
LoadRpc original_load_rpc = nullptr;
LoadBytes original_load_bytes = nullptr;
std::atomic<std::uint64_t> master_revision{0};
thread_local FanMultiplierMemo memo;

struct MasterChange {
  MasterChange() { master_revision.fetch_add(1, std::memory_order_acq_rel); }
  ~MasterChange() { master_revision.fetch_add(1, std::memory_order_acq_rel); }
};

double CachedMultiplier(void* owner, std::int32_t area, const void* method) {
  if (!owner || area < 1 || area > 2) return original_multiplier(owner, area, method);
  const auto revision = master_revision.load(std::memory_order_acquire);
  const auto level = fan_level(owner, area, nullptr);
  const auto key = reinterpret_cast<std::uintptr_t>(owner);
  double value;
  if (memo.Find(key, area, level, revision, value)) return value;
  value = original_multiplier(owner, area, method);
  // A nested table change must not cache a partially loaded result.
  if (master_revision.load(std::memory_order_acquire) == revision)
    memo.Store(key, area, level, revision, value);
  return value;
}
void ClearMaster(void* owner, const void* method) {
  const MasterChange change;
  original_clear(owner, method);
}
void SetMasterRow(void* owner, std::int32_t id, void* row, const void* method) {
  const MasterChange change;
  original_set_row(owner, id, row, method);
}
bool LoadMasterRpc(void* owner, void* data, const void* method) {
  const MasterChange change;
  return original_load_rpc(owner, data, method);
}
NullableBool LoadMasterBytes(void* owner, void* data, const void* method) {
  const MasterChange change;
  return original_load_bytes(owner, data, method);
}
}  // namespace

bool InitializeFanMultiplierCache(std::uintptr_t base) {
  for (const auto& dependency : kGeneratedDependencies)
    if (dependency.name == "gameplay.fan.level")
      fan_level = reinterpret_cast<Level>(base + dependency.rva);
  // Both shipped ABIs must declare the entire cache + invalidation boundary.
  unsigned mask = 0;
  for (const auto& target : kGeneratedHookTargets) {
    if (target.name == "gameplay.fan.multiplier") mask |= 1;
    if (target.name == "gameplay.fan.clear") mask |= 2;
    if (target.name == "gameplay.fan.setRow") mask |= 4;
    if (target.name == "gameplay.fan.loadRpc") mask |= 8;
    if (target.name == "gameplay.fan.loadBytes") mask |= 16;
  }
  master_revision.fetch_add(1, std::memory_order_acq_rel);
  return fan_level != nullptr && mask == 31;
}

HookBinding ResolveFanMultiplierCacheHook(std::string_view name) {
  if (name == "gameplay.fan.multiplier")
    return {reinterpret_cast<void*>(CachedMultiplier), reinterpret_cast<void**>(&original_multiplier)};
  if (name == "gameplay.fan.clear")
    return {reinterpret_cast<void*>(ClearMaster), reinterpret_cast<void**>(&original_clear)};
  if (name == "gameplay.fan.setRow")
    return {reinterpret_cast<void*>(SetMasterRow), reinterpret_cast<void**>(&original_set_row)};
  if (name == "gameplay.fan.loadRpc")
    return {reinterpret_cast<void*>(LoadMasterRpc), reinterpret_cast<void**>(&original_load_rpc)};
  if (name == "gameplay.fan.loadBytes")
    return {reinterpret_cast<void*>(LoadMasterBytes), reinterpret_cast<void**>(&original_load_bytes)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
