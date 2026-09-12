#include "ggfm/gameplay_compat.hpp"
#include "ggfm/client_save.hpp"
#include "ggfm/gameplay_rules.hpp"
#include "ggfm/upgrade_increment.hpp"
#include "ggfm/follower_row_cache.hpp"
#include "ggfm/fan_multiplier_cache.hpp"
#include "ggfm/runtime.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <android/log.h>

namespace ggfm {
namespace {
#include "ggfm/generated_hook_targets.inc"

constexpr std::uintptr_t Field(std::string_view name) {
  for (const auto& field : kGeneratedFieldOffsets) {
    if (field.name == name) return field.offset;
  }
  return 0;
}

constexpr auto kPassEnd = Field("pass.window.endTicks");
constexpr auto kQuestClaims = Field("quest.response.received1");
static_assert(kPassEnd != 0 && kQuestClaims != 0);

using SetWindow = void (*)(void*, int, int, const void*);
using UpdateQuest = void (*)(void*, void*, const void*);
using GetQuestState = int (*)(void*, int, const void*);
SetWindow original_set_window = nullptr;
UpdateQuest original_update_quest = nullptr;
GetQuestState original_get_quest_state = nullptr;
using BindMusic = bool (*)(void*, const void*);
BindMusic original_bind_music = nullptr;
using LoadMusic = bool (*)(void*, void*, bool, const void*);
LoadMusic original_load_music = nullptr;
LoadMusic original_load_costume = nullptr;
LoadMusic original_load_ad_level = nullptr;
using GetTable = void* (*)(const void*);
GetTable get_table = nullptr;
using BuyApClick = void (*)(void*, const void*);
BuyApClick original_buy_ap_click = nullptr;
using MasterConstructor = void (*)(void*, void*, const void*);
MasterConstructor original_skill_constructor = nullptr;
MasterConstructor original_unit_constructor = nullptr;
EncodeUpgradeFloat encode_upgrade_float = nullptr;
static_assert(Field("upgrade.row.increment") != 0);
static_assert(Field("upgrade.skillDto.increment") != 0);
static_assert(Field("upgrade.unitDto.increment") != 0);

void RestoreMasterIncrement(void* row, void* dto, std::string_view kind,
                            std::uintptr_t source_offset) {
  // Stock RPC constructors truncate double -> int -> float. SQL and binary
  // cache readers preserve their stored float (including an old cached zero).
  // Repair just this field after the original
  // constructor, using the game's value constructor (including its state).
  const bool restored = RestoreUpgradeIncrement(row, dto, source_offset,
      Field("upgrade.row.increment"), encode_upgrade_float);
  double source_increment = 0;
  if (dto != nullptr)
    std::memcpy(&source_increment, static_cast<char*>(dto) + source_offset, sizeof(source_increment));
  __android_log_print(restored ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "GGFM",
      "master: %.*s RPC upgrade increment source=%.9g float32=%.9g written=%d",
      static_cast<int>(kind.size()), kind.data(), source_increment,
      static_cast<double>(static_cast<float>(source_increment)), restored);
}

void SkillConstructor(void* row, void* dto, const void* method) {
  original_skill_constructor(row, dto, method);
  RestoreMasterIncrement(row, dto, "skill", Field("upgrade.skillDto.increment"));
}

void UnitConstructor(void* row, void* dto, const void* method) {
  original_unit_constructor(row, dto, method);
  RestoreMasterIncrement(row, dto, "unit", Field("upgrade.unitDto.increment"));
}

void RetiredBuyApClick(void*, const void*) {
  // Same boundary as the tested LSPosed adapter: the stock retired BuyAP
  // popup dereferences absent store data before sending any RPC. This guard
  // affects only its plus-button click, not CH3 entry, AP recovery or scoring.
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "ch3: retired BuyAP popup suppressed; local AP recovery remains active");
}

bool LoadBoundContent(LoadMusic load, std::string_view domain, std::uintptr_t owner_offset,
                      void* instance, void* list, bool refresh, const void* method) {
  void* table = get_table(nullptr);
  bool initialized = false;
  void* active_manager = nullptr;
  if (table != nullptr) {
    std::memcpy(&initialized, static_cast<char*>(table) + Field("table.initialized"), sizeof(initialized));
    std::memcpy(&active_manager, static_cast<char*>(table) + owner_offset, sizeof(active_manager));
  }
  const bool effective_refresh = RefreshLoadedContent(
      refresh, initialized, instance != nullptr && instance == active_manager);
  // Login uses refresh=false because its first response precedes InitTableData.
  // A later full response must take the existing stock refresh branch as well:
  // it reconstructs rows, binds their master references, and calculates caches.
  // No IDs, levels, ownership, or master pointers are synthesized here.
  const bool result = load(instance, list, effective_refresh, method);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "content: %.*s loaded refresh=%d effective=%d tables_ready=%d success=%d",
                      static_cast<int>(domain.size()), domain.data(), refresh,
                      effective_refresh, initialized, result);
  return result;
}

bool MusicLoad(void* instance, void* list, bool refresh, const void* method) {
  return LoadBoundContent(original_load_music, "music", Field("table.music"),
                          instance, list, refresh, method);
}

bool CostumeLoad(void* instance, void* list, bool refresh, const void* method) {
  return LoadBoundContent(original_load_costume, "costume", Field("table.costume"),
                          instance, list, refresh, method);
}

bool AdLevelLoad(void* instance, void* list, bool refresh, const void* method) {
  return LoadBoundContent(original_load_ad_level, "ad-level", Field("table.adLevel"),
                          instance, list, refresh, method);
}

bool AuditMusicBinding(void* instance, const void* method) {
  const bool result = original_bind_music(instance, method);
  if (instance != nullptr) {
    void* master = nullptr;
    std::memcpy(&master, static_cast<char*>(instance) + Field("music.master"), sizeof(master));
    __android_log_print(result ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "GGFM",
                        "music: bind record=%p master=%p success=%d",
                        instance, master, result);
  }
  return result;
}

// Unity invokes both callbacks on its main thread. Every server update replaces
// all three flags, including zeros, so a completed stage cannot bleed forward.
QuestClaimProjection claims;

void DailyPassWindow(void* instance, int year, int group_count, const void* method) {
  original_set_window(instance, year, group_count, method);
  if (instance == nullptr) return;
  const auto context = GetRequestContext();
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  // Both adjacent DateTime fields use the stock UTC+9 comparison clock, not
  // device wall time. Leave the selected season and active flag unchanged.
  const auto start = PassDayStartTicks(now, context.utc_offset_minutes);
  const auto end = PassDayEndTicks(now, context.utc_offset_minutes);
  std::memcpy(static_cast<char*>(instance) + kPassEnd - sizeof(start), &start, sizeof(start));
  std::memcpy(static_cast<char*>(instance) + kPassEnd, &end, sizeof(end));
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
      "pass: daily window offset=%d start=%lld end=%lld (stock UTC+9 ticks)",
      context.utc_offset_minutes, static_cast<long long>(start), static_cast<long long>(end));
}

void RestoreQuestClaims(void* instance, void* response, const void* method) {
  std::array<std::int16_t, 3> flags{};
  if (response != nullptr) {
    std::memcpy(flags.data(), static_cast<char*>(response) + kQuestClaims,
                flags.size() * sizeof(flags[0]));
  }
  claims.Update(GetRequestContext().usn, flags);
  original_update_quest(instance, response, method);
}

int QuestClaimState(void* instance, int index, const void* method) {
  const int state = original_get_quest_state(instance, index, method);
  return instance != nullptr ? claims.State(GetRequestContext().usn, index, state)
                             : state;
}
}  // namespace

bool InitializeGameplayCompatibility(const std::uintptr_t il2cpp_base) {
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == "gameplay.upgrade.encodeFloat") {
      encode_upgrade_float = reinterpret_cast<EncodeUpgradeFloat>(il2cpp_base + dependency.rva);
    }
    if (dependency.name == "gameplay.table.instance") {
      get_table = reinterpret_cast<GetTable>(il2cpp_base + dependency.rva);
    }
  }
  return get_table != nullptr && encode_upgrade_float != nullptr && InitializeClientSave(il2cpp_base) &&
         InitializeFanMultiplierCache(il2cpp_base);
}

HookBinding ResolveGameplayCompatibilityHook(std::string_view name) {
  if (name == "gameplay.upgrade.skillConstructor")
    return {reinterpret_cast<void*>(SkillConstructor), reinterpret_cast<void**>(&original_skill_constructor)};
  if (name == "gameplay.upgrade.unitConstructor")
    return {reinterpret_cast<void*>(UnitConstructor), reinterpret_cast<void**>(&original_unit_constructor)};
  if (name.starts_with("gameplay.save.")) return ResolveClientSaveHook(name);
  if (name.starts_with("gameplay.followerRow")) return ResolveFollowerRowCacheHook(name);
  if (name.starts_with("gameplay.fan.")) return ResolveFanMultiplierCacheHook(name);
  if (name == "gameplay.ch3.buyApGuard")
    return {reinterpret_cast<void*>(RetiredBuyApClick),
            reinterpret_cast<void**>(&original_buy_ap_click)};
  if (name == "gameplay.music.load")
    return {reinterpret_cast<void*>(MusicLoad), reinterpret_cast<void**>(&original_load_music)};
  if (name == "gameplay.costume.load")
    return {reinterpret_cast<void*>(CostumeLoad), reinterpret_cast<void**>(&original_load_costume)};
  if (name == "gameplay.adLevel.load")
    return {reinterpret_cast<void*>(AdLevelLoad), reinterpret_cast<void**>(&original_load_ad_level)};
  if (name == "gameplay.music.auditBinding")
    return {reinterpret_cast<void*>(AuditMusicBinding),
            reinterpret_cast<void**>(&original_bind_music)};
  if (name == "gameplay.pass.setWindow")
    return {reinterpret_cast<void*>(DailyPassWindow),
            reinterpret_cast<void**>(&original_set_window)};
  if (name == "gameplay.quest.update")
    return {reinterpret_cast<void*>(RestoreQuestClaims),
            reinterpret_cast<void**>(&original_update_quest)};
  if (name == "gameplay.quest.getState")
    return {reinterpret_cast<void*>(QuestClaimState),
            reinterpret_cast<void**>(&original_get_quest_state)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
