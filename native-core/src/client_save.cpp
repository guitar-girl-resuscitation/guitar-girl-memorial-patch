#include "ggfm/client_save.hpp"
#include "ggfm/runtime.hpp"

#include <android/log.h>

namespace ggfm {
namespace {
#include "ggfm/generated_hook_targets.inc"

using GetManager = void* (*)(const void*);
using Upload = void (*)(void*, int, void*, void*, bool, bool, bool, const void*);
using CharacterUpgrade = bool (*)(void*, int, int, const void*);
using FollowerUpgrade = bool (*)(void*, int, int, bool, bool, const void*);
GetManager get_manager = nullptr;
Upload upload = nullptr;
CharacterUpgrade original_character_upgrade = nullptr;
FollowerUpgrade original_follower_upgrade = nullptr;

void SaveSuccessfulUpgrade(const char* kind, int id, int count) {
  // Called on Unity's thread after the stock qualification, deduction and
  // level update. Do not invent levels or bypass failed purchases. Resolve
  // the live manager each time; no player object survives a slot change.
  if (GetRequestContext().usn <= 0) return;
  auto* manager = get_manager(nullptr);
  if (!manager) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM", "save: no live DataManager after %s upgrade", kind);
    return;
  }
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "save: queue full snapshot after %s id=%d count=%d", kind, id, count);
  // SaveDataType.ALL=1. Force collection, not a cached periodic snapshot.
  // Include currency/achievement changes in the same server transaction as
  // the level; a character-only upload would leave its deduction unsaved.
  // This queues the stock RPC: completion is proven by the server commit log,
  // not by this function returning. No waiting overlay or background thread.
  upload(manager, 1, nullptr, nullptr, false, false, true, nullptr);
}

bool UpgradeCharacter(void* manager, int id, int count, const void* method) {
  return SaveAfterLocalSuccess(
      [&] { return original_character_upgrade(manager, id, count, method); },
      [&] { SaveSuccessfulUpgrade("character", id, count); });
}

bool UpgradeFollower(void* manager, int id, int count, bool option1, bool option2,
                     const void* method) {
  return SaveAfterLocalSuccess(
      [&] { return original_follower_upgrade(manager, id, count, option1, option2, method); },
      [&] { SaveSuccessfulUpgrade("follower", id, count); });
}
}  // namespace

bool InitializeClientSave(std::uintptr_t il2cpp_base) {
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == "gameplay.save.manager")
      get_manager = reinterpret_cast<GetManager>(il2cpp_base + dependency.rva);
    if (dependency.name == "gameplay.save.upload")
      upload = reinterpret_cast<Upload>(il2cpp_base + dependency.rva);
  }
  return get_manager && upload;
}

HookBinding ResolveClientSaveHook(std::string_view name) {
  if (name == "gameplay.save.characterUpgrade")
    return {reinterpret_cast<void*>(UpgradeCharacter), reinterpret_cast<void**>(&original_character_upgrade)};
  if (name == "gameplay.save.followerUpgrade")
    return {reinterpret_cast<void*>(UpgradeFollower), reinterpret_cast<void**>(&original_follower_upgrade)};
  return {nullptr, nullptr};
}
}  // namespace ggfm
