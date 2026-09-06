#include "ggfm/runtime_hooks.hpp"

#include "ggfm/runtime.hpp"
#include "ggfm/memorial_ui.hpp"
#include "ggfm/gameplay_compat.hpp"
#include "ggfm/admin_bridge.hpp"
#include "ggfm/offline_sdk.hpp"

#include <android/log.h>
#include <dlfcn.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace ggfm {
namespace {
#include "ggfm/generated_hook_targets.inc"

constexpr std::uintptr_t RuntimeField(std::string_view name) {
  for (const auto& field : kGeneratedFieldOffsets) {
    if (field.name == name) return field.offset;
  }
  return 0;
}
static_assert(RuntimeField("sdk.firebase.initialized") != 0);
static_assert(RuntimeField("sdk.googleAds.initialized") != 0);
static_assert(RuntimeField("sdk.appsflyer.initialized") != 0);
static_assert(RuntimeField("notice.maintenance.code") != 0);
static_assert(RuntimeField("offline.minSeconds") != 0);
static_assert(RuntimeField("offline.maxSeconds") != 0);
static_assert(RuntimeField("offline.startTicks") != 0);
static_assert(RuntimeField("offline.elapsedTicks") != 0);

struct Il2CppString {
  void* klass;
  void* monitor;
  std::int32_t length;
  char16_t chars[1];
};

using StringNew = Il2CppString* (*)(const char*);
using StringNewUtf16 = Il2CppString* (*)(const char16_t*, std::int32_t);
StringNew string_new = nullptr;
StringNewUtf16 string_new_utf16 = nullptr;
std::uintptr_t set_header_address = 0;
std::uintptr_t get_url_address = 0;
std::uintptr_t do_server_purchase_address = 0;
std::uintptr_t request_update_time_address = 0;

using UrlGetter = Il2CppString* (*)(void*, const void*);
using SendRequest = void* (*)(void*, const void*);
using SetHeader = void (*)(void*, Il2CppString*, Il2CppString*, const void*);
using GetUrl = Il2CppString* (*)(void*, const void*);
using SetInt = void (*)(Il2CppString*, std::int32_t, const void*);
using GetInt = std::int32_t (*)(Il2CppString*, std::int32_t, const void*);
using SetFloat = void (*)(Il2CppString*, float, const void*);
using GetFloat = float (*)(Il2CppString*, float, const void*);
using SetString = void (*)(Il2CppString*, Il2CppString*, const void*);
using GetString = Il2CppString* (*)(Il2CppString*, Il2CppString*, const void*);
using HasKey = bool (*)(Il2CppString*, const void*);
using DeleteKey = void (*)(Il2CppString*, const void*);
using ObjectGetClass = void* (*)(void*);
using ClassGetMethod = const void* (*)(void*, const char*, int);
using ClassGetField = void* (*)(void*, const char*);
using FieldSetValue = void (*)(void*, void*, void*);
using FieldGetValue = void (*)(void*, void*, void*);
using ObjectUnbox = void* (*)(void*);
using RuntimeInvoke = void* (*)(const void*, void*, void**, void**);
using MaintenanceHandler = bool (*)(void*, void*, void*, const void*);
using MaintenancePopup = void (*)(void*, void**, void*, void*, const void*);
using ManagedTransition = void (*)(void*, const void*);
using PopupOpenAnimation = void (*)(void*, void*, const void*);

UrlGetter unused_url_original = nullptr;
PopupOpenAnimation original_popup_open_animation = nullptr;
SendRequest original_send = nullptr;
SetInt original_set_int = nullptr;
GetInt original_get_int = nullptr;
SetFloat original_set_float = nullptr;
GetFloat original_get_float = nullptr;
SetString original_set_string = nullptr;
GetString original_get_string = nullptr;
HasKey original_has_key = nullptr;
DeleteKey original_delete_key = nullptr;
ObjectGetClass object_get_class = nullptr;
ClassGetMethod class_get_method = nullptr;
ClassGetField class_get_field = nullptr;
FieldSetValue field_set_value = nullptr;
FieldGetValue field_get_value = nullptr;
ObjectUnbox object_unbox = nullptr;
RuntimeInvoke runtime_invoke = nullptr;
MaintenanceHandler original_maintenance_handler_object = nullptr;
MaintenanceHandler original_maintenance_handler_shared = nullptr;
MaintenancePopup original_maintenance_popup = nullptr;
ManagedTransition original_go_to_ingame = nullptr;
ManagedTransition original_go_to_event_intro = nullptr;
ManagedTransition original_offline_calculate = nullptr;
ManagedTransition original_offline_elapsed = nullptr;
ManagedTransition original_offline_reward = nullptr;

void LogOfflineState(const char* phase, void* instance) {
  if (instance == nullptr) return;
  // Audited v8 HibernationManagerSGT fields; diagnostics do not change timing.
  std::int32_t minimum = 0, maximum = 0;
  std::int64_t start = 0, elapsed = 0;
  const auto* bytes = static_cast<const char*>(instance);
  std::memcpy(&minimum, bytes + RuntimeField("offline.minSeconds"), sizeof(minimum));
  std::memcpy(&maximum, bytes + RuntimeField("offline.maxSeconds"), sizeof(maximum));
  std::memcpy(&start, bytes + RuntimeField("offline.startTicks"), sizeof(start));
  std::memcpy(&elapsed, bytes + RuntimeField("offline.elapsedTicks"), sizeof(elapsed));
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
      "offline: %s min_seconds=%d max_seconds=%d start_ticks=%lld elapsed_ticks=%lld",
      phase, minimum, maximum, static_cast<long long>(start), static_cast<long long>(elapsed));
}

void OfflineCalculate(void* instance, const void* method) {
  LogOfflineState("calculate", instance);
  original_offline_calculate(instance, method);
}
void OfflineElapsed(void* instance, const void* method) {
  original_offline_elapsed(instance, method);
  LogOfflineState("elapsed", instance);
}
void OfflineReward(void* instance, const void* method) {
  LogOfflineState("reward", instance);
  original_offline_reward(instance, method);
}
std::atomic<std::uint32_t> game_url_call_count{0};
std::atomic<std::uint32_t> cdn_url_call_count{0};
std::atomic<std::uint32_t> request_send_call_count{0};
std::atomic_bool memorial_notice_seen_this_process{false};
std::atomic_bool memorial_notice_refresh_requested{false};

bool InvokeDelegate(void* delegate, void** arguments, const int argument_count) {
  if (delegate == nullptr || object_get_class == nullptr ||
      class_get_method == nullptr || runtime_invoke == nullptr) {
    return false;
  }
  auto* klass = object_get_class(delegate);
  const auto* invoke = class_get_method(klass, "Invoke", argument_count);
  if (invoke == nullptr) return false;
  void* exception = nullptr;
  runtime_invoke(invoke, delegate, arguments, &exception);
  return exception == nullptr;
}

Il2CppString* Managed(const std::string& value) {
  return string_new == nullptr ? nullptr : string_new(value.c_str());
}

std::string Utf8ForLog(const Il2CppString* value) {
  if (value == nullptr || value->length < 0) return "<null>";
  std::string result;
  const auto length = static_cast<std::size_t>(value->length);
  result.reserve(length > 512 ? 512 : length);
  for (std::size_t index = 0; index < length && index < 512; ++index) {
    const auto character = value->chars[index];
    result.push_back(character <= 0x7f ? static_cast<char>(character) : '?');
  }
  return result;
}

Il2CppString* Namespaced(Il2CppString* key) {
  if (key == nullptr || string_new_utf16 == nullptr || key->length < 0) return key;
  const std::u16string original(key->chars, key->chars + key->length);
  const auto changed = NamespacePlayerPrefsKey(std::u16string_view(original));
  if (changed == original) return key;
  return string_new_utf16(changed.data(), static_cast<std::int32_t>(changed.size()));
}

Il2CppString* ReturnGameUrl(void*, const void*) {
  const auto context = GetRequestContext();
  const auto call = game_url_call_count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (call <= 32) {
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: game URL getter call #%u -> %s", call,
                        context.endpoint.c_str());
  }
  return Managed(context.endpoint);
}

Il2CppString* ReturnCdnUrl(void*, const void*) {
  // AssetBundles stay in the user-supplied package. No public CDN is needed.
  const auto context = GetRequestContext();
  const auto call = cdn_url_call_count.fetch_add(1, std::memory_order_relaxed) + 1;
  if (call <= 8) {
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: CDN URL getter call #%u -> %s/AssetBundles",
                        call, context.endpoint.c_str());
  }
  return Managed(context.endpoint + "/AssetBundles");
}

Il2CppString* ReturnCompatibleApplicationVersion(const void*) {
  // GlobalManagerSGT in 8.0.0 removes dots from Application.version and then
  // parses the whole result as Int32. The memorial Android versionName keeps
  // its human-readable suffix, so expose the pinned source version only to
  // this legacy managed compatibility surface.
  return Managed("8.0.0");
}

void* SendWithMemorialHeaders(void* request, const void* method) {
  const auto context = GetRequestContext();
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();
  const auto set_header = reinterpret_cast<SetHeader>(set_header_address);
  const auto sequence = NextRequestSequence();
  const auto call = request_send_call_count.fetch_add(1, std::memory_order_relaxed) + 1;
  const auto get_url = reinterpret_cast<GetUrl>(get_url_address);
  const auto request_url = Utf8ForLog(get_url(request, nullptr));
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send call #%u seq=%llu url=%s endpoint=%s request=%p",
                      call, static_cast<unsigned long long>(sequence),
                      request_url.c_str(), context.endpoint.c_str(), request);

  // AssetBundle requests also use UnityWebRequest.  Memorial authentication is
  // scoped to the embedded HTTP endpoint and must never be attached to file://
  // or jar:file:// loads.
  if (!request_url.starts_with(context.endpoint)) {
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: request.send #%u bypassing memorial headers",
                        call);
    auto* operation = original_send(request, method);
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: request.send #%u returned operation=%p", call,
                        operation);
    return operation;
  }

  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u setting session header", call);
  set_header(request, Managed("X-GGFM-Session"), Managed(context.capability), nullptr);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u setting sequence header", call);
  set_header(request, Managed("X-GGFM-Request-Seq"),
             Managed(std::to_string(sequence)), nullptr);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u setting device-time header", call);
  set_header(request, Managed("X-GGFM-Device-Time"), Managed(std::to_string(now)), nullptr);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u setting utc-offset header", call);
  set_header(request, Managed("X-GGFM-UTC-Offset"),
             Managed(std::to_string(context.utc_offset_minutes)), nullptr);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u dispatching Unity request", call);
  auto* operation = original_send(request, method);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: request.send #%u returned operation=%p", call,
                      operation);
  return operation;
}

void SetIntHook(Il2CppString* key, std::int32_t value, const void* method) {
  original_set_int(Namespaced(key), value, method);
}
std::int32_t GetIntHook(Il2CppString* key, std::int32_t fallback, const void* method) {
  return original_get_int(Namespaced(key), fallback, method);
}
void SetFloatHook(Il2CppString* key, float value, const void* method) {
  original_set_float(Namespaced(key), value, method);
}
float GetFloatHook(Il2CppString* key, float fallback, const void* method) {
  return original_get_float(Namespaced(key), fallback, method);
}
void SetStringHook(Il2CppString* key, Il2CppString* value, const void* method) {
  original_set_string(Namespaced(key), value, method);
  const auto name = Utf8ForLog(key);
  if (name == "Key_HibernationStartTimeTicks" || name == "Key_HibernationTotalTimeTicks") {
    static std::atomic<unsigned> logged{0};
    if (logged.fetch_add(1) < 32)
      __android_log_print(ANDROID_LOG_INFO, "GGFM", "offline: prefs write %s=%s readback=%s", name.c_str(), Utf8ForLog(value).c_str(),
                          Utf8ForLog(original_get_string(Namespaced(key), Managed("<missing>"), nullptr)).c_str());
  }
}
Il2CppString* GetStringHook(Il2CppString* key, Il2CppString* fallback, const void* method) {
  auto* result = original_get_string(Namespaced(key), fallback, method);
  const auto name = Utf8ForLog(key);
  if (name == "Key_HibernationStartTimeTicks" || name == "Key_HibernationTotalTimeTicks")
    __android_log_print(ANDROID_LOG_INFO, "GGFM", "offline: prefs read %s=%s", name.c_str(), Utf8ForLog(result).c_str());
  return result;
}
bool HasKeyHook(Il2CppString* key, const void* method) {
  return original_has_key(Namespaced(key), method);
}
void DeleteKeyHook(Il2CppString* key, const void* method) {
  const auto name = Utf8ForLog(key);
  if (name == "Key_HibernationStartTimeTicks" || name == "Key_HibernationTotalTimeTicks")
    __android_log_print(ANDROID_LOG_INFO, "GGFM", "offline: prefs delete %s", name.c_str());
  original_delete_key(Namespaced(key), method);
}
void DeleteAllHook(const void*) {
  // A global delete would erase every slot. Slot deletion is implemented by
  // the memorial save manager, which enumerates only its USN prefix.
}
// PlayerPrefs.Save remains native: key isolation is performed by the read/write
// hooks. A forwarding-only hook adds no behavior and needlessly relocates the
// ARMv7 internal-call resolver's PC-relative loads.

void RewardedAdHook(std::int32_t, void* on_success, void*, const void*) {
  InvokeDelegate(on_success, nullptr, 0);
}

void NoReadyAdHook(void*, std::int32_t, void* on_success, void*, const void*) {
  // Some callers take the stock unavailable-ad branch before RequestAndShow.
  // Preserve the tested LSPosed completion path, without its countdown popup.
  InvokeDelegate(on_success, nullptr, 0);
}

bool InstallLocalMembership(void* manager) {
  const auto fail = [](const char* stage) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM", "identity: local membership failed stage=%s", stage);
    return false;
  };
  if (!manager) return fail("manager-null");
  const auto context = GetRequestContext();
  const auto member_id = LocalMemberId(context.usn);
  if (member_id.empty()) return fail("local-session-identity");
  auto* klass = object_get_class(manager);
  // Resolve all members before changing any state. The pinned stock commit
  // sets ApiServer's login type/member/token/guest/conflict and PPLogin=true.
  // Calling only success (rev75) leaves those fields unset and causes re-login.
  const auto* commit = class_get_method(klass, "뀅뀍뀏뀐뀍뀋뀁뀐뀃뀃뀐", 4);
  const auto* check = class_get_method(klass, "IsLoggedIn", 0);
  std::array<void*, 4> fields{
      class_get_field(klass, "ppMemberId"), class_get_field(klass, "ppAccessToken"),
      class_get_field(klass, "ppConflictMemberId"), class_get_field(klass, "ppIsGuestLogin")};
  if (!commit || !check) return fail("method-resolution");
  for (auto* field : fields) if (!field) return fail("field-resolution");
  auto* member = Managed(member_id);
  // This is a local compatibility marker, not a publisher credential or the
  // HTTP capability. It must never be usable outside the embedded endpoint.
  auto* token = Managed("ggfm-local-session");
  auto* conflict = Managed("");
  if (!member || !token || !conflict) return fail("managed-strings");
  bool guest = true;
  // In this pinned IL2CPP API, instance reference fields take the object
  // directly (SetValueRaw(..., deref=false)), unlike value-type fields.
  // Passing &member stores a stack address which later crashes GC traversal.
  field_set_value(manager, fields[0], member);
  field_set_value(manager, fields[1], token);
  field_set_value(manager, fields[2], conflict);
  field_set_value(manager, fields[3], &guest);
  void* arguments[]{&guest, member, token, conflict};
  void* exception = nullptr;
  runtime_invoke(commit, manager, arguments, &exception);
  if (exception) return fail("stock-commit-exception");
  // IsLoggedIn returns a boxed bool. Invoke through IL2CPP so a missing
  // dependency is handled as failure, not swallowed as a successful login.
  void* boxed = runtime_invoke(check, manager, nullptr, &exception);
  if (exception) return fail("logged-in-check-exception");
  if (!boxed) return fail("logged-in-check-null");
  auto* value = static_cast<bool*>(object_unbox(boxed));
  return value && *value ? true : fail("logged-in-check-false");
}

void RetiredFirebaseUserIdHook(Il2CppString*, const void*) {
  // Local identity still commits through the stock membership method. Only
  // the retired telemetry sink is suppressed; never log its identifier.
}
void RetiredFirebasePropertyHook(Il2CppString*, Il2CppString*, const void*) {}

void PmangMembershipLoginHook(void* manager, void* on_success, void* on_failure, const void*) {
  const bool ready = InstallLocalMembership(manager);
  __android_log_print(ready ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "GGFM",
                      "identity: local membership state committed=%d", ready);
  InvokeDelegate(ready ? on_success : on_failure, nullptr, 0);
}

void PopupOpenAnimationHook(void* animation, void* on_loaded, const void* method) {
  // The stock open coroutine defers the popup's loaded callback until a fixed
  // lead-in plus the prefab's own tween duration have elapsed. Until it runs,
  // the popup layer answers no touch at all -- confirm button and dimmed
  // background alike -- even though the card reaches its final geometry much
  // earlier, so a player who presses at a natural speed loses that press and
  // has to press a second time. Play the stock animation with no deferred
  // callback, and mark the popup loaded at once so the first press on a fully
  // drawn popup is honoured. Nothing about the animation itself changes.
  original_popup_open_animation(animation, nullptr, method);
  InvokeDelegate(on_loaded, nullptr, 0);
}

void DoStorePurchaseHook(void* manager, void* purchase_data, const void*) {
  using DoServerPurchase = void (*)(void*, void*, const void*);
  reinterpret_cast<DoServerPurchase>(do_server_purchase_address)(
      manager, purchase_data, nullptr);
}

void RetryPendingPlatformOrdersHook(void*, void* handler, const void*) {
  // Complete the SDK query, leaving the game's own JSON parser, waiting-layer
  // cleanup and entry-state continuation intact. Local SQLite purchases are
  // separate atomic transactions and must not be replayed as platform orders.
  bool delivered = false;
  if (handler != nullptr) {
    auto* field = class_get_field(object_get_class(handler), "Success");
    void* listener = nullptr;
    if (field != nullptr) field_get_value(handler, field, &listener);
    auto* payload = Managed(kEmptyPendingPlatformOrders);
    void* arguments[] = {payload};
    if (payload != nullptr) delivered = InvokeDelegate(listener, arguments, 1);
  }
  __android_log_print(delivered ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "GGFM",
                      "sdk: pending platform orders=0; continuation delivered=%d",
                      delivered);
}

void FirebaseManagerInitializeHook(void* manager, const void*) {
  // The original async initializer enters the retired Firebase Messaging
  // native SDK. Its Java peer is intentionally disabled in the memorial
  // package, so allowing that call hands Firebase a null Android object and
  // aborts the process. These services do not carry gameplay state.
  // The verified initialized flag is a byte at offset 0x24 in the pinned
  // 8.0.0 IL2CPP build. Start() waits on this bit before allowing
  // the opening scene to advance, so a plain no-op would leave a permanent
  // black screen. DependencyStatus defaults to Available (zero); only the
  // completion bit needs to be committed here.
  if (manager != nullptr) {
    *reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(manager) + RuntimeField("sdk.firebase.initialized")) = 1;
  }
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: retired Firebase manager satisfied locally");
}

void FirebaseAnalyticsLogEventHook() {
  // Firebase Analytics is intentionally absent from the memorial build.  The
  // generated C# wrapper throws when LogEvent reaches the uninitialised native
  // SDK, and many UI callbacks record an event before performing their actual
  // action.  Intercept every public LogEvent overload at the managed IL2CPP
  // boundary so telemetry remains a deterministic no-op without aborting the
  // rest of the button callback.  A no-argument replacement is ABI-safe here:
  // all overloads return void and the unused AArch64 argument registers are
  // caller-owned.
}

void GoogleMobileAdsManagerInitializeHook(void* manager, const void*) {
  // MainManagerSGT.WaitingInitialize waits on
  // GoogleMobileAdsManagerSGT.get_Initialized, which is exactly the byte at
  // +0x69 in the pinned 8.0.0 build. The stock initializer cannot complete
  // after the retired Google Mobile Ads components are disabled, so satisfy
  // the client-side lifecycle without starting the remote SDK.
  if (manager != nullptr) {
    *reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(manager) + RuntimeField("sdk.googleAds.initialized")) = 1;
  }
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: retired Google Mobile Ads manager satisfied locally");
}

void AppsFlyerManagerInitializeHook(void* manager, const void*) {
  // AppsFlyer is not a gameplay dependency. Preserve the manager's local
  // completion bit while keeping the retired attribution SDK offline.
  if (manager != nullptr) {
    *reinterpret_cast<std::uint8_t*>(
        reinterpret_cast<std::uintptr_t>(manager) + RuntimeField("sdk.appsflyer.initialized")) = 1;
  }
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: retired AppsFlyer manager satisfied locally");
}

Il2CppString* LoadNoticeDontShowDayHook(void*, const void*) {
  // The stock client suppresses the startup notice after persisting the local
  // calendar day. The memorial notice is intentionally a once-per-process
  // disclosure, so each cold start must observe an empty suppression value.
  return Managed("");
}

std::int16_t MaintenanceCode(const void* maintenance) {
  if (maintenance == nullptr) return 0;
  return *reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<std::uintptr_t>(maintenance) + RuntimeField("notice.maintenance.code"));
}

bool ContinueAfterMemorialNoticeObject(void* instance, void* request,
                                       void* maintenance,
                                       const void* method) {
  const auto code = MaintenanceCode(maintenance);
  const bool handled = original_maintenance_handler_object != nullptr
                           ? original_maintenance_handler_object(
                                 instance, request, maintenance, method)
                           : false;
  if (code == 1) {
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: memorial notice RPC continues after popup");
    return false;
  }
  return handled;
}

bool ContinueAfterMemorialNoticeShared(void* instance, void* request,
                                       void* maintenance,
                                       const void* method) {
  const auto code = MaintenanceCode(maintenance);
  const bool handled = original_maintenance_handler_shared != nullptr
                           ? original_maintenance_handler_shared(
                                 instance, request, maintenance, method)
                           : false;
  if (code == 1) {
    __android_log_print(
        ANDROID_LOG_INFO, "GGFM",
        "runtime: shared memorial notice RPC continues after popup");
    return false;
  }
  return handled;
}

void OpenMemorialNoticeWithoutShutdown(void* instance, void** maintenance,
                                       void* on_ok, void* on_cancel,
                                       const void* method) {
  void* value = maintenance == nullptr ? nullptr : *maintenance;
  if (MaintenanceCode(value) == 1) {
    if (memorial_notice_seen_this_process.exchange(true,
                                                   std::memory_order_acq_rel)) {
      __android_log_print(ANDROID_LOG_INFO, "GGFM",
                          "runtime: duplicate memorial notice suppressed");
      return;
    }
    on_ok = nullptr;
    on_cancel = nullptr;
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: memorial notice shutdown callbacks suppressed");
  }
  if (original_maintenance_popup != nullptr) {
    original_maintenance_popup(instance, maintenance, on_ok, on_cancel, method);
  }
}

void RefreshMemorialNoticeIfNeeded(void* instance) {
  if (memorial_notice_seen_this_process.load(std::memory_order_relaxed) ||
      memorial_notice_refresh_requested.exchange(true,
                                                  std::memory_order_relaxed)) {
    return;
  }
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "runtime: refreshing cached memorial notice");
  reinterpret_cast<ManagedTransition>(request_update_time_address)(instance,
                                                                   nullptr);
}

void GoToInGameWithMemorialNotice(void* instance, const void* method) {
  RefreshMemorialNoticeIfNeeded(instance);
  if (original_go_to_ingame != nullptr) original_go_to_ingame(instance, method);
}

void GoToEventIntroWithMemorialNotice(void* instance, const void* method) {
  RefreshMemorialNoticeIfNeeded(instance);
  if (original_go_to_event_intro != nullptr) {
    original_go_to_event_intro(instance, method);
  }
}

HookBinding Resolve(const std::string_view name) {
  if (name == "offline.calculate")
    return {reinterpret_cast<void*>(OfflineCalculate), reinterpret_cast<void**>(&original_offline_calculate)};
  if (name == "offline.elapsed")
    return {reinterpret_cast<void*>(OfflineElapsed), reinterpret_cast<void**>(&original_offline_elapsed)};
  if (name == "offline.reward")
    return {reinterpret_cast<void*>(OfflineReward), reinterpret_cast<void**>(&original_offline_reward)};
  if (name.starts_with("gameplay.")) return ResolveGameplayCompatibilityHook(name);
  if (name.starts_with("server.") && name != "server.cdn")
    return {reinterpret_cast<void*>(ReturnGameUrl),
            reinterpret_cast<void**>(&unused_url_original)};
  if (name == "server.cdn")
    return {reinterpret_cast<void*>(ReturnCdnUrl),
            reinterpret_cast<void**>(&unused_url_original)};
  if (name == "request.send")
    return {reinterpret_cast<void*>(SendWithMemorialHeaders),
            reinterpret_cast<void**>(&original_send)};
  if (name == "compat.applicationVersion")
    return {reinterpret_cast<void*>(ReturnCompatibleApplicationVersion), nullptr};
  if (name == "ads.requestAndShow")
    return {reinterpret_cast<void*>(RewardedAdHook), nullptr};
  if (name == "ads.noReady")
    return {reinterpret_cast<void*>(NoReadyAdHook), nullptr};
  if (name == "billing.doStorePurchase")
    return {reinterpret_cast<void*>(DoStorePurchaseHook), nullptr};
  if (name == "ui.popupOpenAnimation")
    return {reinterpret_cast<void*>(PopupOpenAnimationHook),
            reinterpret_cast<void**>(&original_popup_open_animation)};
  if (name == "billing.pendingPlatformOrders")
    return {reinterpret_cast<void*>(RetryPendingPlatformOrdersHook), nullptr};
  if (name == "firebase.managerInitialize")
    return {reinterpret_cast<void*>(FirebaseManagerInitializeHook), nullptr};
  if (name.starts_with("firebase.analytics.logEvent."))
    return {reinterpret_cast<void*>(FirebaseAnalyticsLogEventHook), nullptr};
  if (name == "firebase.analytics.setUserId" || name == "firebase.crashlytics.setUserId")
    return {reinterpret_cast<void*>(RetiredFirebaseUserIdHook), nullptr};
  if (name == "firebase.analytics.setUserProperty")
    return {reinterpret_cast<void*>(RetiredFirebasePropertyHook), nullptr};
  if (name == "ads.managerInitialize")
    return {reinterpret_cast<void*>(GoogleMobileAdsManagerInitializeHook), nullptr};
  if (name == "appsflyer.managerInitialize")
    return {reinterpret_cast<void*>(AppsFlyerManagerInitializeHook), nullptr};
  if (name == "pmang.membershipLogin")
    return {reinterpret_cast<void*>(PmangMembershipLoginHook), nullptr};
  if (name == "notice.loadDontShowDay")
    return {reinterpret_cast<void*>(LoadNoticeDontShowDayHook), nullptr};
  if (name == "notice.maintenanceHandlerObject")
    return {reinterpret_cast<void*>(ContinueAfterMemorialNoticeObject),
            reinterpret_cast<void**>(&original_maintenance_handler_object)};
  if (name == "notice.maintenanceHandlerShared")
    return {reinterpret_cast<void*>(ContinueAfterMemorialNoticeShared),
            reinterpret_cast<void**>(&original_maintenance_handler_shared)};
  if (name == "notice.maintenancePopup")
    return {reinterpret_cast<void*>(OpenMemorialNoticeWithoutShutdown),
            reinterpret_cast<void**>(&original_maintenance_popup)};
  if (name == "notice.goToInGame")
    return {reinterpret_cast<void*>(GoToInGameWithMemorialNotice),
            reinterpret_cast<void**>(&original_go_to_ingame)};
  if (name == "notice.goToEventIntro")
    return {reinterpret_cast<void*>(GoToEventIntroWithMemorialNotice),
            reinterpret_cast<void**>(&original_go_to_event_intro)};
  if (name.starts_with("ui.setting.") || name.starts_with("ui.mailbox."))
    return ResolveMemorialUiHook(name);
  if (name == "prefs.setInt") return {reinterpret_cast<void*>(SetIntHook), reinterpret_cast<void**>(&original_set_int)};
  if (name == "prefs.getInt") return {reinterpret_cast<void*>(GetIntHook), reinterpret_cast<void**>(&original_get_int)};
  if (name == "prefs.setFloat") return {reinterpret_cast<void*>(SetFloatHook), reinterpret_cast<void**>(&original_set_float)};
  if (name == "prefs.getFloat") return {reinterpret_cast<void*>(GetFloatHook), reinterpret_cast<void**>(&original_get_float)};
  if (name == "prefs.setString") return {reinterpret_cast<void*>(SetStringHook), reinterpret_cast<void**>(&original_set_string)};
  if (name == "prefs.getString") return {reinterpret_cast<void*>(GetStringHook), reinterpret_cast<void**>(&original_get_string)};
  if (name == "prefs.hasKey") return {reinterpret_cast<void*>(HasKeyHook), reinterpret_cast<void**>(&original_has_key)};
  if (name == "prefs.deleteKey") return {reinterpret_cast<void*>(DeleteKeyHook), reinterpret_cast<void**>(&original_delete_key)};
  if (name == "prefs.deleteAll") return {reinterpret_cast<void*>(DeleteAllHook), nullptr};
  return {nullptr, nullptr};
}


const FingerprintTarget* Dependency(const std::string_view name) {
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == name) return &dependency;
  }
  return nullptr;
}
}  // namespace

bool InstallRuntimeHooks(HookBackend& backend, const std::uintptr_t il2cpp_base,
                         void* il2cpp_handle,
                         const bool firebase_only_diagnostic) {
  void* symbol_scope = il2cpp_handle == nullptr ? RTLD_DEFAULT : il2cpp_handle;
  if (!firebase_only_diagnostic && !InitializeMemorialUiApi(il2cpp_handle, il2cpp_base)) return false;
  string_new = reinterpret_cast<StringNew>(dlsym(symbol_scope, "il2cpp_string_new"));
  string_new_utf16 = reinterpret_cast<StringNewUtf16>(
      dlsym(symbol_scope, "il2cpp_string_new_utf16"));
  object_get_class = reinterpret_cast<ObjectGetClass>(
      dlsym(symbol_scope, "il2cpp_object_get_class"));
  class_get_method = reinterpret_cast<ClassGetMethod>(
      dlsym(symbol_scope, "il2cpp_class_get_method_from_name"));
  class_get_field = reinterpret_cast<ClassGetField>(
      dlsym(symbol_scope, "il2cpp_class_get_field_from_name"));
  field_set_value = reinterpret_cast<FieldSetValue>(
      dlsym(symbol_scope, "il2cpp_field_set_value"));
  field_get_value = reinterpret_cast<FieldGetValue>(
      dlsym(symbol_scope, "il2cpp_field_get_value"));
  object_unbox = reinterpret_cast<ObjectUnbox>(dlsym(symbol_scope, "il2cpp_object_unbox"));
  runtime_invoke = reinterpret_cast<RuntimeInvoke>(
      dlsym(symbol_scope, "il2cpp_runtime_invoke"));
  const auto* set_header = Dependency("request.setHeader");
  const auto* get_url = Dependency("request.getUrl");
  const auto* do_server_purchase = Dependency("billing.doServerPurchase");
  const auto* request_update_time = Dependency("notice.requestUpdateTime");
  if (string_new == nullptr || string_new_utf16 == nullptr ||
      object_get_class == nullptr || class_get_method == nullptr ||
      runtime_invoke == nullptr || class_get_field == nullptr || field_set_value == nullptr ||
      field_get_value == nullptr || object_unbox == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "runtime: required exported IL2CPP API is unavailable");
    return false;
  }
  if (!firebase_only_diagnostic &&
      (set_header == nullptr || get_url == nullptr ||
       do_server_purchase == nullptr || request_update_time == nullptr)) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "runtime: required dependency target is absent");
    return false;
  }
  if (!firebase_only_diagnostic &&
      !ValidateFingerprints(il2cpp_base, kGeneratedDependencies)) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "runtime: dependency fingerprint mismatch");
    return false;
  }
  if (firebase_only_diagnostic) {
    bool installed = true;
    std::size_t installed_count = 0;
    for (const auto& target : kGeneratedHookTargets) {
      if (target.name != "firebase.managerInitialize" &&
          target.name != "ads.managerInitialize" &&
          target.name != "appsflyer.managerInitialize" &&
          target.name != "compat.applicationVersion")
        continue;
      installed = InstallHooks(backend, il2cpp_base,
                               std::span<const HookTarget>(&target, 1), Resolve) &&
                  installed;
      ++installed_count;
    }
    if (installed_count != 4) {
      __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                          "runtime: startup SDK diagnostic targets are incomplete");
      return false;
    }
    __android_log_print(ANDROID_LOG_WARN, "GGFM",
                        "runtime: retired startup SDK Hook profile active");
    return installed;
  }
  set_header_address = il2cpp_base + set_header->rva;
  get_url_address = il2cpp_base + get_url->rva;
  do_server_purchase_address = il2cpp_base + do_server_purchase->rva;
  request_update_time_address = il2cpp_base + request_update_time->rva;
  if (!InitializeGameplayCompatibility(il2cpp_base)) return false;
  return InstallHooks(backend, il2cpp_base, kGeneratedHookTargets, Resolve);
}

}  // namespace ggfm
