#include "ggfm/gameplay_compat.hpp"
#include "ggfm/admin_bridge.hpp"
#include "ggfm/client_save.hpp"
#include "ggfm/fan_multiplier_cache.hpp"
#include "ggfm/gameplay_rules.hpp"
#include "ggfm/ch3_preview.hpp"
#include "ggfm/upgrade_increment.hpp"
#include "ggfm/runtime.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <android/log.h>

namespace ggfm {
namespace {
extern "C" int32_t ggfm_ch3_guest_bonus(int64_t, uint32_t, int32_t, int32_t);
using PreviewStage = void (*)(void*, const void*);
using GuestScore = int (*)(void*, int, const void*);
PreviewStage original_preview = nullptr;
GuestScore original_guest_score = nullptr;
thread_local ch3::Preview* current_preview = nullptr;

void PreviewStageScore(void* panel, const void* method) {
  auto preview = ch3::snapshot(panel);
  struct Scope {
    ch3::Preview* previous;
    ~Scope() { current_preview = previous; }
  } scope{current_preview};
  current_preview = &preview;
  original_preview(panel, method);
}

int PreviewGuestScore(void* manager, int level, const void* method) {
  const int base = original_guest_score(manager, level, method);
  auto* preview = current_preview;
  if (!preview || preview->next >= preview->count) return base;
  const auto guest = preview->guests[preview->next++];
  if (!guest.hinted || guest.level != level) return base;
  return base + ggfm_ch3_guest_bonus(preview->stage, preview->hints, level, base);
}
#include "ggfm/generated_hook_targets.inc"

constexpr std::uintptr_t Field(std::string_view name) {
  for (const auto& field : kGeneratedFieldOffsets) {
    if (field.name == name) return field.offset;
  }
  return 0;
}

constexpr auto kPassEnd = Field("pass.window.endTicks");
constexpr auto kPassId = Field("pass.window.id");
constexpr auto kPassActive = Field("pass.window.active");
static_assert(kPassId != 0 && kPassActive != 0);
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
  auto* row = static_cast<char*>(instance);
  int id = 0;
  std::memcpy(&id, row + kPassId, sizeof(id));
  if (id < 1 || id > 13 || context.pass_anchor_season < 1 || context.pass_anchor_season > 13) return;
  const int season = DailyPassSeason(now, context.utc_offset_minutes, context.pass_anchor_day,
                                     context.pass_anchor_season);
  const bool active = id == season;
  std::memcpy(row + kPassActive, &active, sizeof(active));
  if (!active) return;
  const auto start = PassDayStartTicks(now, context.utc_offset_minutes);
  const auto end = PassDayEndTicks(now, context.utc_offset_minutes);
  std::memcpy(static_cast<char*>(instance) + kPassEnd - sizeof(start), &start, sizeof(start));
  std::memcpy(static_cast<char*>(instance) + kPassEnd, &end, sizeof(end));
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
      "pass: active subscription=%d offset=%d start=%lld end=%lld (stock UTC+9 ticks)",
      id, context.utc_offset_minutes, static_cast<long long>(start), static_cast<long long>(end));
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

using ShowGuide = void (*)(void*, void*, const void*);
ShowGuide original_show_guide = nullptr;
GetTable get_guide_manager = nullptr;
PreviewStage notify_guide_ad = nullptr;

void ShowGuideWithCommittedProgress(void* panel, void* quest, const void* method) {
  constexpr auto p = sizeof(void*);
  void* user = nullptr;
  void* master = nullptr;
  if (quest) {
    std::memcpy(&user, static_cast<char*>(quest) + 2*p + 8, p);
    std::memcpy(&master, static_cast<char*>(quest) + 3*p + 8, p);
  }
  int stage = 0;
  if (user) std::memcpy(&stage, static_cast<char*>(user) + 2*p + 4, sizeof(stage));
  if (stage == 4 && master && QuestClaimState(quest, 2, nullptr) == 0) {
    const auto reply = AdminRequest("GET", "/memorial/v1/events/teacher-ad");
    if (reply.status == 200 && reply.body == "rewardedAdCompleted") {
      if (void* manager = get_guide_manager(nullptr)) notify_guide_ad(manager, nullptr);
    }
  }
  original_show_guide(panel, quest, method);
}
}  // namespace

bool InitializeGameplayCompatibility(const std::uintptr_t il2cpp_base) {
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == "ads.questManager")
      get_guide_manager = reinterpret_cast<GetTable>(il2cpp_base + dependency.rva);
    if (dependency.name == "ads.questViewed")
      notify_guide_ad = reinterpret_cast<PreviewStage>(il2cpp_base + dependency.rva);
    if (dependency.name == "gameplay.upgrade.encodeFloat") {
      encode_upgrade_float = reinterpret_cast<EncodeUpgradeFloat>(il2cpp_base + dependency.rva);
    }
    if (dependency.name == "gameplay.table.instance") {
      get_table = reinterpret_cast<GetTable>(il2cpp_base + dependency.rva);
    }
  }
  return get_guide_manager && notify_guide_ad && get_table && encode_upgrade_float && InitializeClientSave(il2cpp_base) &&
         InitializeFanMultiplierCache(il2cpp_base);
}

HookBinding ResolveGameplayCompatibilityHook(std::string_view name) {
  if (name == "ads.questShow")
    return {reinterpret_cast<void*>(ShowGuideWithCommittedProgress), reinterpret_cast<void**>(&original_show_guide)};
  if (name == "gameplay.upgrade.skillConstructor")
    return {reinterpret_cast<void*>(SkillConstructor), reinterpret_cast<void**>(&original_skill_constructor)};
  if (name == "gameplay.upgrade.unitConstructor")
    return {reinterpret_cast<void*>(UnitConstructor), reinterpret_cast<void**>(&original_unit_constructor)};
  if (name.starts_with("gameplay.save.")) return ResolveClientSaveHook(name);
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
  if (name == "gameplay.ch3.preview")
    return {reinterpret_cast<void*>(PreviewStageScore), reinterpret_cast<void**>(&original_preview)};
  if (name == "gameplay.ch3.guestScore")
    return {reinterpret_cast<void*>(PreviewGuestScore), reinterpret_cast<void**>(&original_guest_score)};
  if (name == "gameplay.quest.update")
    return {reinterpret_cast<void*>(RestoreQuestClaims),
            reinterpret_cast<void**>(&original_update_quest)};
  if (name == "gameplay.quest.getState")
    return {reinterpret_cast<void*>(QuestClaimState),
            reinterpret_cast<void**>(&original_get_quest_state)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
