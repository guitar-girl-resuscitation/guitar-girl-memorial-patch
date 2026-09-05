#include "ggfm/memorial_ui.hpp"

#include "ggfm/admin_bridge.hpp"
#include "ggfm/menu_selection.hpp"
#include "ggfm/il2cpp_gc_handle.hpp"

#include <dlfcn.h>
#include <android/log.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cinttypes>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ggfm {
namespace {
struct Il2CppString {
  void* klass;
  void* monitor;
  std::int32_t length;
  char16_t chars[1];
};

struct LocaleText {
  std::string_view locale;
  std::string_view menu;
  std::string_view legacy;
  std::string_view currency;
  std::string_view pass;
  std::string_view saves;
  std::string_view close;
  std::string_view more;
  std::string_view back;
  std::string_view previous;
  std::string_view next;
  std::string_view send;
  std::string_view outfit;
  std::string_view guitar;
  std::string_view music;
  std::string_view ch1;
  std::string_view ch2;
  std::string_view about;
  std::string_view sent;
  std::string_view owned;
  std::string_view error;
  std::string_view restart;
  std::string_view create;
  std::string_view remove;
  std::string_view switch_save;
  std::string_view active;
  std::string_view pending;
  std::string_view about_description;
};

#include "ggfm/generated_localization.inc"
#include "ggfm/generated_hook_targets.inc"

using StringNew = Il2CppString* (*)(const char*);
using DomainGet = void* (*)();
using DomainAssemblies = const void** (*)(void*, std::size_t*);
using AssemblyImage = void* (*)(const void*);
using ClassFromName = void* (*)(void*, const char*, const char*);
using ClassGetMethod = const void* (*)(void*, const char*, int);
using ClassGetType = const void* (*)(void*);
using TypeGetObject = void* (*)(const void*);
using RuntimeInvoke = void* (*)(const void*, void*, void**, void**);
using VoidInstance = void (*)(void*, const void*);
using ObjectUnbox = void* (*)(void*);
using ClassGetField = void* (*)(void*, const char*);
using FieldSetValue = void (*)(void*, void*, void*);

StringNew string_new = nullptr;
DomainGet domain_get = nullptr;
DomainAssemblies domain_assemblies = nullptr;
AssemblyImage assembly_image = nullptr;
ClassFromName class_from_name = nullptr;
ClassGetMethod class_get_method = nullptr;
ClassGetType class_get_type = nullptr;
TypeGetObject type_get_object = nullptr;
RuntimeInvoke runtime_invoke = nullptr;
WeakHandleNew weak_handle_new = nullptr;
HandleTarget handle_target = nullptr;
HandleFree handle_free = nullptr;
ObjectUnbox object_unbox = nullptr;
ClassGetField class_get_field = nullptr;
FieldSetValue field_set_value = nullptr;
Il2CppGcHandle mailbox_handle = 0;
bool pending_mail_refresh = false;
VoidInstance request_post_time = nullptr;
void* label_type = nullptr;
void* game_object_class = nullptr;
void* main_manager_class = nullptr;
void* setting_class = nullptr;
void* application_class = nullptr;

VoidInstance original_show = nullptr;
VoidInstance original_refresh = nullptr;
VoidInstance original_enable = nullptr;
VoidInstance original_disable = nullptr;
VoidInstance original_mailbox_start = nullptr;
VoidInstance original_auth_visibility = nullptr;
VoidInstance original_login = nullptr;
VoidInstance original_logout = nullptr;
VoidInstance original_restore = nullptr;
VoidInstance original_help = nullptr;
VoidInstance original_privacy = nullptr;

enum class Page {
  Normal,
  Root,
  More,
  LegacyArea,
  LegacyKind,
  LegacyList,
  CurrencyList,
  CurrencyAmount,
  PassList,
  SaveRoot,
  SaveList,
  SaveDeleteList,
  Restart,
};

enum class Action {
  None,
  Enter,
  Close,
  BackRoot,
  BackMore,
  Legacy,
  Currency,
  More,
  Pass,
  Saves,
  SaveList,
  SaveCreate,
  SaveDelete,
  DeleteCurrent,
  BackSaves,
  About,
  Area1,
  Area2,
  Costume,
  Guitar,
  Music,
  Previous,
  Next,
  Select,
  Amount100,
  Amount1K,
  Amount1A,
  Restart,
};

struct LegacyItem {
  std::string kind;
  int area = 1;
  std::int64_t id = 0;
  std::string name;
  bool can_send = false;
};

struct SaveSlot {
  std::int64_t usn = 0;
  std::string name;
  bool active = false;
  bool pending = false;
};

struct MenuState {
  void* manager = nullptr;
  Page page = Page::Normal;
  std::array<Action, 4> actions{};
  int area = 1;
  std::string kind;
  std::size_t index = 0;
  std::vector<LegacyItem> legacy;
  std::vector<std::string> currencies;
  std::vector<int> seasons;
  std::vector<SaveSlot> saves;
  std::string selected_currency;
};

MenuState menu;
MenuSelectionQueue legacy_selection;
MenuAmountQueue currency_amount;

std::uintptr_t FieldOffset(const std::string_view name) {
  const auto found = std::find_if(
      kGeneratedFieldOffsets.begin(), kGeneratedFieldOffsets.end(),
      [name](const auto& field) { return field.name == name; });
  return found == kGeneratedFieldOffsets.end() ? 0 : found->offset;
}

const std::array<std::size_t, 4>& AuthObjects() {
  static const std::array<std::size_t, 4> offsets = {
      FieldOffset("setting.row.auth.online"),
      FieldOffset("setting.row.auth.logoutOnline"),
      FieldOffset("setting.row.auth.offline"),
      FieldOffset("setting.row.auth.logoutOffline")};
  return offsets;
}
const std::array<std::size_t, 2>& RestoreObjects() {
  static const std::array<std::size_t, 2> offsets = {
      FieldOffset("setting.row.restore.online"),
      FieldOffset("setting.row.restore.offline")};
  return offsets;
}

template <typename T>
T Symbol(void* scope, const char* name) {
  return reinterpret_cast<T>(dlsym(scope, name));
}

bool ApiReady() {
  return string_new != nullptr && domain_get != nullptr && domain_assemblies != nullptr &&
         assembly_image != nullptr && class_from_name != nullptr &&
         class_get_method != nullptr && class_get_type != nullptr &&
         type_get_object != nullptr && runtime_invoke != nullptr &&
         weak_handle_new != nullptr && handle_target != nullptr && handle_free != nullptr &&
         request_post_time != nullptr && object_unbox != nullptr &&
         class_get_field != nullptr && field_set_value != nullptr;
}

void* FindClass(const char* namespc, const char* name) {
  auto* domain = domain_get();
  std::size_t count = 0;
  const void** assemblies = domain_assemblies(domain, &count);
  for (std::size_t index = 0; index < count; ++index) {
    auto* image = assembly_image(assemblies[index]);
    if (auto* klass = class_from_name(image, namespc, name); klass != nullptr) return klass;
  }
  return nullptr;
}

bool ResolveTypes() {
  if (!ApiReady()) return false;
  if (label_type != nullptr) return true;
  auto* label_class = FindClass("", "UILabel");
  game_object_class = FindClass("UnityEngine", "GameObject");
  application_class = FindClass("UnityEngine", "Application");
  main_manager_class = FindClass("", "GgUIInGameMainManagerSGT");
  setting_class = FindClass("", "GgUIInGameSettingManager");
  if (label_class == nullptr || game_object_class == nullptr || application_class == nullptr ||
      main_manager_class == nullptr || setting_class == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "ui: missing managed types label=%p gameObject=%p application=%p main=%p settings=%p",
                        label_class, game_object_class, application_class,
                        main_manager_class, setting_class);
    return false;
  }
  label_type = type_get_object(class_get_type(label_class));
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "ui: managed types ready=%d",
                      label_type != nullptr);
  return label_type != nullptr;
}

void* Invoke(const void* method, void* instance, void** arguments) {
  if (method == nullptr) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM", "ui: managed method was not resolved");
    return nullptr;
  }
  void* exception = nullptr;
  void* result = runtime_invoke(method, instance, arguments, &exception);
  if (exception != nullptr)
    __android_log_print(ANDROID_LOG_ERROR, "GGFM", "ui: managed invocation failed method=%p exception=%p", method, exception);
  return exception == nullptr ? result : nullptr;
}

Il2CppString* Managed(const std::string_view value) {
  return string_new == nullptr ? nullptr : string_new(std::string(value).c_str());
}

void SetObjectLabel(void* game_object, const std::string_view value) {
  if (game_object == nullptr || !ResolveTypes()) return;
  const auto* get_label = class_get_method(
      game_object_class, "GetComponentInChildren", 2);
  bool include_inactive = true;
  void* get_arguments[] = {label_type, &include_inactive};
  auto* label = Invoke(get_label, game_object, get_arguments);
  if (label == nullptr) {
    __android_log_print(ANDROID_LOG_WARN, "GGFM", "ui: row has no UILabel object=%p getter=%p",
                        game_object, get_label);
    return;
  }
  // Only these replacement labels belong to us. blueasa's global Refresh()
  // visits disabled localizers too, so disabling the component alone does not
  // prevent its delayed Start/Refresh from restoring the original Login text.
  // Its enum value 0 explicitly skips translation (verified in 8.0.0).
  for (const auto* namespc : {"blueasa", ""}) {
    auto* localize_class = FindClass(namespc, "UILocalize");
    if (!localize_class) continue;
    auto* localize_type = type_get_object(class_get_type(localize_class));
    void* arguments[] = {localize_type, &include_inactive};
    auto* localizer = Invoke(get_label, game_object, arguments);
    if (!localizer) continue;
    if (auto* field = class_get_field(localize_class, "m_eString")) {
      std::int32_t no_translation = 0;
      field_set_value(localizer, field, &no_translation);
    }
    auto* behaviour = FindClass("UnityEngine", "Behaviour");
    bool enabled = false;
    void* enabled_arguments[] = {&enabled};
    if (behaviour) Invoke(class_get_method(behaviour, "set_enabled", 1),
                          localizer, enabled_arguments);
  }
  auto* label_class = *reinterpret_cast<void**>(label);
  const auto* set_text = class_get_method(label_class, "set_text", 1);
  auto* text = Managed(value);
  void* set_arguments[] = {text};
  Invoke(set_text, label, set_arguments);
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "ui: row label object=%p label=%p setter=%p text=%s",
                      game_object, label, set_text, std::string(value).c_str());
}

void* ObjectAt(void* manager, const std::size_t offset) {
  if (manager == nullptr || offset == 0) return nullptr;
  return *reinterpret_cast<void**>(
      reinterpret_cast<std::uintptr_t>(manager) + offset);
}

void SetObjectActive(void* game_object, const bool active) {
  if (game_object == nullptr || !ResolveTypes()) return;
  const auto* set_active = class_get_method(game_object_class, "SetActive", 1);
  void* arguments[] = {const_cast<bool*>(&active)};
  Invoke(set_active, game_object, arguments);
}

// Bounded diagnostic: an enabled button can still be hidden by a parent or
// covered by another platform's row. Inspect actual hierarchy, not just labels.
void TraceRow(void* object, const char* row) {
  if (object == nullptr || !ResolveTypes()) return;
  auto* transform_class = FindClass("UnityEngine", "Transform");
  auto* component_class = FindClass("UnityEngine", "Component");
  auto* object_class = FindClass("UnityEngine", "Object");
  if (!transform_class || !component_class || !object_class) return;
  const auto* get_transform = class_get_method(game_object_class, "get_transform", 0);
  const auto* get_object = class_get_method(component_class, "get_gameObject", 0);
  const auto* get_parent = class_get_method(transform_class, "get_parent", 0);
  const auto* get_position = class_get_method(transform_class, "get_localPosition", 0);
  const auto* get_name = class_get_method(object_class, "get_name", 0);
  const auto* get_active = class_get_method(game_object_class, "get_activeSelf", 0);
  for (int depth = 0; object != nullptr && depth < 5; ++depth) {
    auto* name = static_cast<Il2CppString*>(Invoke(get_name, object, nullptr));
    std::string printable;
    if (name) for (int i = 0; i < name->length && i < 80; ++i)
      printable.push_back(name->chars[i] < 128 ? static_cast<char>(name->chars[i]) : '?');
    auto* active = Invoke(get_active, object, nullptr);
    auto* transform = Invoke(get_transform, object, nullptr);
    auto* position = transform ? Invoke(get_position, transform, nullptr) : nullptr;
    const auto* xyz = position ? static_cast<float*>(object_unbox(position)) : nullptr;
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "ui: hierarchy row=%s depth=%d name=%s active=%d local=(%.1f,%.1f) object=%p",
                        row, depth, printable.c_str(), active && *static_cast<bool*>(object_unbox(active)),
                        xyz ? xyz[0] : 0.0f, xyz ? xyz[1] : 0.0f, object);
    auto* parent = transform ? Invoke(get_parent, transform, nullptr) : nullptr;
    object = parent ? Invoke(get_object, parent, nullptr) : nullptr;
  }
}

void PrepareMemorialRows(void* manager) {
  if (manager == nullptr) return;

  // Offline variants are non-interactive placeholders with a crossed-network
  // mask. Reuse the interactive variants with our local callbacks instead.
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.onlineParent")), true);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.offlineParent")), false);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.online")), true);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.logoutOnline")), false);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.offline")), false);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.auth.logoutOffline")), false);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.restore.online")), false);
  SetObjectActive(ObjectAt(manager, FieldOffset("setting.row.restore.offline")), false);

  // The stock Android login and iOS restore containers occupy the SAME slot.
  // Enabling both covers the memorial entry with Restore Purchases. Keep the
  // iOS container hidden. Menu pages use an independent Android panel and
  // never change the other Settings rows or relocate their transforms.
  if (!ResolveTypes()) return;
  auto* transform_class = FindClass("UnityEngine", "Transform");
  auto* component_class = FindClass("UnityEngine", "Component");
  if (!transform_class || !component_class) return;
  const auto* get_transform = class_get_method(game_object_class, "get_transform", 0);
  const auto* get_parent = class_get_method(transform_class, "get_parent", 0);
  const auto* get_object = class_get_method(component_class, "get_gameObject", 0);
  auto* restore = ObjectAt(manager, FieldOffset("setting.row.restore.online"));
  auto* restore_transform = restore ? Invoke(get_transform, restore, nullptr) : nullptr;
  auto* parent = restore_transform ? Invoke(get_parent, restore_transform, nullptr) : nullptr;
  if (!parent) return;
  auto* parent_object = Invoke(get_object, parent, nullptr);
  SetObjectActive(parent_object, false);
}

template <std::size_t Size>
void SetRow(void* manager, const std::array<std::size_t, Size>& offsets,
            const std::string_view value) {
  if (manager == nullptr) return;
  for (const auto offset : offsets) {
    SetObjectLabel(ObjectAt(manager, offset), value);
  }
}

const LocaleText& Text(void* manager) {
  struct Choice {
    std::size_t offset;
    std::string_view locale;
  };
  const std::array<Choice, 12> choices = {{
      {FieldOffset("setting.language.ko"), "ko"},
      {FieldOffset("setting.language.en"), "en"},
      {FieldOffset("setting.language.ja"), "ja"},
      {FieldOffset("setting.language.zh-Hans"), "zh-Hans"},
      {FieldOffset("setting.language.zh-Hant"), "zh-Hant"},
      {FieldOffset("setting.language.vi"), "vi"},
      {FieldOffset("setting.language.es"), "es"},
      {FieldOffset("setting.language.it"), "it"},
      {FieldOffset("setting.language.id"), "id"},
      {FieldOffset("setting.language.th"), "th"},
      {FieldOffset("setting.language.pt"), "pt"},
      {FieldOffset("setting.language.hi"), "hi"},
  }};
  std::string_view selected = "en";
  if (manager != nullptr) {
    for (const auto& choice : choices) {
      auto* toggle = *reinterpret_cast<void**>(
          reinterpret_cast<std::uintptr_t>(manager) + choice.offset);
      // UIToggle.value reads startsActive before Start(), mIsActive afterwards.
      // Reading mIsActive directly misidentifies the first-open language as KO.
      auto* value = toggle ? Invoke(class_get_method(
          *reinterpret_cast<void**>(toggle), "get_value", 0), toggle, nullptr) : nullptr;
      if (value != nullptr && *static_cast<bool*>(object_unbox(value))) {
        selected = choice.locale;
        break;
      }
    }
  }
  const auto found = std::find_if(kGeneratedLocales.begin(), kGeneratedLocales.end(),
                                  [selected](const auto& row) {
                                    return row.locale == selected;
                                  });
  return found == kGeneratedLocales.end() ? kGeneratedLocales[0] : *found;
}

void Render(std::array<std::string, 4> labels,
            const std::array<Action, 4> actions) {
  menu.actions = actions;
  ShowMemorialPage(Text(menu.manager).menu, labels, Text(menu.manager).close);
}

void ShowRoot() {
  const auto& text = Text(menu.manager);
  menu.page = Page::Root;
  Render({std::string(text.close), std::string(text.legacy),
          std::string(text.currency), std::string(text.more)},
         {Action::Close, Action::Legacy, Action::Currency, Action::More});
}

void ShowMore() {
  const auto& text = Text(menu.manager);
  menu.page = Page::More;
  Render({std::string(text.back), std::string(text.pass), std::string(text.saves),
          std::string(text.about)},
         {Action::BackRoot, Action::Pass, Action::Saves, Action::About});
}

void ShowSaveRoot() {
  const auto& text = Text(menu.manager);
  menu.page = Page::SaveRoot;
  Render({std::string(text.back), std::string(text.switch_save),
          std::string(text.create), std::string(text.remove)},
         {Action::BackMore, Action::SaveList, Action::SaveCreate,
          Action::SaveDelete});
}

void ShowRestartPage(const Action back) {
  const auto& text = Text(menu.manager);
  menu.page = Page::Restart;
  Render({std::string(text.back), std::string(text.restart),
          std::string(text.close), std::string(text.close)},
         {back, Action::Restart, Action::Close, Action::Close});
}

void QuitGame() {
  if (!ResolveTypes() || application_class == nullptr) return;
  if (const auto* quit = class_get_method(application_class, "Quit", 0);
      quit != nullptr) {
    Invoke(quit, nullptr, nullptr);
    return;
  }
  if (const auto* quit = class_get_method(application_class, "Quit", 1);
      quit != nullptr) {
    std::int32_t exit_code = 0;
    void* arguments[] = {&exit_code};
    Invoke(quit, nullptr, arguments);
  }
}

void ShowLegacyArea() {
  const auto& text = Text(menu.manager);
  menu.page = Page::LegacyArea;
  Render({std::string(text.back), std::string(text.ch1), std::string(text.ch2),
          std::string(text.close)},
         {Action::BackRoot, Action::Area1, Action::Area2, Action::Close});
}

void ShowLegacyKind() {
  const auto& text = Text(menu.manager);
  menu.page = Page::LegacyKind;
  Render({std::string(text.back), std::string(text.outfit), std::string(text.guitar),
          std::string(text.music)},
         {Action::Legacy, Action::Costume, Action::Guitar, Action::Music});
}

std::optional<std::string> JsonString(const std::string_view object,
                                      const std::string_view key) {
  const auto marker = std::string("\"") + std::string(key) + "\":";
  auto start = object.find(marker);
  if (start == std::string_view::npos) return std::nullopt;
  start += marker.size();
  while (start < object.size() && object[start] == ' ') ++start;
  if (start >= object.size() || object[start] != '"') return std::nullopt;
  ++start;
  std::string result;
  for (std::size_t cursor = start; cursor < object.size(); ++cursor) {
    const char value = object[cursor];
    if (value == '"') return result;
    if (value == '\\' && cursor + 1 < object.size()) {
      const char escaped = object[++cursor];
      if (escaped == 'n') result.push_back('\n');
      else if (escaped == 'r') result.push_back('\r');
      else if (escaped == 't') result.push_back('\t');
      else result.push_back(escaped);
    } else {
      result.push_back(value);
    }
  }
  return std::nullopt;
}

std::optional<std::int64_t> JsonInteger(const std::string_view object,
                                        const std::string_view key) {
  const auto marker = std::string("\"") + std::string(key) + "\":";
  auto start = object.find(marker);
  if (start == std::string_view::npos) return std::nullopt;
  start += marker.size();
  while (start < object.size() && object[start] == ' ') ++start;
  auto end = start;
  while (end < object.size() && (object[end] == '-' ||
         (object[end] >= '0' && object[end] <= '9'))) ++end;
  std::int64_t value = 0;
  const auto parsed = std::from_chars(object.data() + start, object.data() + end, value);
  return parsed.ec == std::errc{} ? std::optional(value) : std::nullopt;
}

std::vector<std::string_view> JsonObjects(const std::string& json) {
  std::vector<std::string_view> result;
  bool quoted = false;
  bool escaped = false;
  int depth = 0;
  std::size_t start = 0;
  for (std::size_t index = 0; index < json.size(); ++index) {
    const char value = json[index];
    if (quoted) {
      if (escaped) escaped = false;
      else if (value == '\\') escaped = true;
      else if (value == '"') quoted = false;
      continue;
    }
    if (value == '"') quoted = true;
    else if (value == '{') {
      if (depth++ == 0) start = index;
    } else if (value == '}' && depth > 0 && --depth == 0) {
      result.emplace_back(json.data() + start, index - start + 1);
    }
  }
  return result;
}

void Popup(const std::string_view title, const std::string_view description) {
  ShowMemorialMessage(title, description, Text(menu.manager).close);
}

void ShowError(const AdminResponse& response) {
  const auto& text = Text(menu.manager);
  __android_log_print(ANDROID_LOG_WARN, "GGFM", "ui: memorial request failed status=%d", response.status);
  if (response.status == 409) Popup(text.menu, text.owned);
  else Popup(text.menu, text.error);
}

void ShowLegacyList() {
  const auto& text = Text(menu.manager);
  auto response = AdminRequest("GET", "/memorial/v1/legacy-items");
  if (!response.ok()) {
    ShowError(response);
    return;
  }
  menu.legacy.clear();
  const auto locale = Text(menu.manager).locale;
  for (const auto object : JsonObjects(response.body)) {
    const auto kind = JsonString(object, "kind");
    const auto area = JsonInteger(object, "area");
    const auto id = JsonInteger(object, "id");
    if (!kind || !area || !id || *kind != menu.kind || *area != menu.area) continue;
    const auto names_at = object.find("\"names\":");
    auto name = names_at == std::string_view::npos
                    ? std::optional<std::string>{}
                    : JsonString(object.substr(names_at), locale);
    if (!name && names_at != std::string_view::npos)
      name = JsonString(object.substr(names_at), "en");
    menu.legacy.push_back({*kind, static_cast<int>(*area), *id,
                           name.value_or(*kind + " " + std::to_string(*id)),
                           object.find("\"canSend\":true") != std::string_view::npos});
  }
  if (menu.legacy.empty()) {
    Popup(text.legacy, text.error);
    return;
  }
  menu.page = Page::LegacyList;
  menu.actions = {Action::None, Action::None, Action::Select, Action::None};
  std::vector<std::string> labels;
  std::vector<bool> enabled;
  for (const auto& item : menu.legacy) {
    labels.push_back(item.name + "\n" + std::string(item.can_send ? text.send : text.owned));
    enabled.push_back(item.can_send);
  }
  ShowMemorialList(text.legacy, labels, enabled, text.back, text.close, legacy_selection.Publish());
}

void ShowCurrencyList() {
  const auto& text = Text(menu.manager);
  if (menu.currencies.empty()) {
    menu.currencies = {"ch1_like", "ch2_like", "ch2_note", "candy", "chocolate", "fans"};
  }
  currency_amount.Publish();
  menu.page = Page::CurrencyList;
  menu.actions = {Action::BackRoot, Action::None, Action::Select, Action::None};
  ShowMemorialCurrencyList(text.locale, legacy_selection.Publish());
}

void ShowAmountList() {
  const auto& text = Text(menu.manager);
  menu.page = Page::CurrencyAmount;
  legacy_selection.Publish();
  menu.actions = {Action::Currency, Action::None, Action::None, Action::None};
  ShowMemorialCurrencyInput(menu.selected_currency, text.locale, currency_amount.Publish());
}

void ShowPassList() {
  const auto& text = Text(menu.manager);
  if (menu.seasons.empty()) {
    const auto response = AdminRequest("GET", "/memorial/v1/star-pass");
    if (!response.ok()) {
      ShowError(response);
      return;
    }
    const auto start = response.body.find("\"seasons\":[");
    if (start != std::string::npos) {
      auto cursor = start + 11;
      while (cursor < response.body.size() && response.body[cursor] != ']') {
        while (cursor < response.body.size() &&
               (response.body[cursor] == ',' || response.body[cursor] == ' ')) ++cursor;
        int season = 0;
        const auto parsed = std::from_chars(response.body.data() + cursor,
                                            response.body.data() + response.body.size(), season);
        if (parsed.ec != std::errc{}) break;
        menu.seasons.push_back(season);
        cursor = parsed.ptr - response.body.data();
      }
    }
  }
  if (menu.seasons.empty()) {
    Popup(text.pass, text.error);
    return;
  }
  menu.index %= menu.seasons.size();
  menu.page = Page::PassList;
  menu.actions = {Action::BackMore, Action::None, Action::Select, Action::None};
  std::vector<std::string> labels;
  for (const auto season : menu.seasons) {
    labels.push_back(std::string(text.pass) + " " + std::to_string(season));
  }
  ShowMemorialList(std::string(text.pass) + "\n" + std::string(text.restart),
                   labels, std::vector<bool>(labels.size(), true),
                   text.back, text.close, legacy_selection.Publish());
}

void ShowSaveList(const bool deleting = false) {
  const auto& text = Text(menu.manager);
  const auto response = AdminRequest("GET", "/memorial/v1/slots");
  if (!response.ok()) {
    ShowError(response);
    return;
  }
  menu.saves.clear();
  for (const auto object : JsonObjects(response.body)) {
    const auto usn = JsonInteger(object, "usn");
    const auto name = JsonString(object, "displayName");
    if (!usn || !name) continue;
    menu.saves.push_back({*usn, *name, object.find("\"active\":true") != std::string_view::npos,
                          object.find("\"pending\":true") != std::string_view::npos});
  }
  if (menu.saves.empty()) {
    Popup(text.saves, text.error);
    return;
  }
  menu.index %= menu.saves.size();
  const auto& slot = menu.saves[menu.index];
  const std::string marker = slot.active ? " [" + std::string(text.active) + "]"
                                         : (slot.pending ? " [" + std::string(text.pending) + "]" : "");
  menu.page = deleting ? Page::SaveDeleteList : Page::SaveList;
  Render({std::string(text.back), std::string(text.previous),
          slot.name + marker + (deleting ? " · " + std::string(text.remove) : ""),
          std::string(text.next)},
         {Action::BackSaves, Action::Previous,
          deleting ? Action::DeleteCurrent : Action::Select, Action::Next});
}

void Move(const int delta) {
  if (menu.page == Page::LegacyList && !menu.legacy.empty()) {
    menu.index = delta < 0 ? (menu.index + menu.legacy.size() - 1) % menu.legacy.size()
                           : (menu.index + 1) % menu.legacy.size();
    ShowLegacyList();
  } else if (menu.page == Page::CurrencyList && !menu.currencies.empty()) {
    menu.index = delta < 0 ? (menu.index + menu.currencies.size() - 1) % menu.currencies.size()
                           : (menu.index + 1) % menu.currencies.size();
    ShowCurrencyList();
  } else if (menu.page == Page::PassList && !menu.seasons.empty()) {
    menu.index = delta < 0 ? (menu.index + menu.seasons.size() - 1) % menu.seasons.size()
                           : (menu.index + 1) % menu.seasons.size();
    ShowPassList();
  } else if ((menu.page == Page::SaveList || menu.page == Page::SaveDeleteList) &&
             !menu.saves.empty()) {
    menu.index = delta < 0 ? (menu.index + menu.saves.size() - 1) % menu.saves.size()
                           : (menu.index + 1) % menu.saves.size();
    ShowSaveList(menu.page == Page::SaveDeleteList);
  }
}

void SelectCurrent() {
  const auto& text = Text(menu.manager);
  AdminResponse response;
  if (menu.page == Page::LegacyList) {
    if (menu.index >= menu.legacy.size() || !menu.legacy[menu.index].can_send) {
      Popup(text.legacy, text.owned);
      return;
    }
    const auto& item = menu.legacy[menu.index];
    const auto path = "/memorial/v1/legacy-items/" + item.kind + "/" +
                      std::to_string(item.area) + "/" + std::to_string(item.id) + "/mail";
    response = AdminRequest("POST", path);
    if (response.ok()) pending_mail_refresh = true;
    if (response.ok() || response.status == 409) ShowLegacyList();
    response.ok() ? Popup(text.legacy, text.sent) : ShowError(response);
  } else if (menu.page == Page::CurrencyList) {
    menu.selected_currency = menu.currencies[menu.index];
    ShowAmountList();
  } else if (menu.page == Page::PassList) {
    response = AdminRequest("POST", "/memorial/v1/star-pass/" +
                                        std::to_string(menu.seasons[menu.index]) + "/select");
    if (response.ok()) {
      ShowRestartPage(Action::BackMore);
      Popup(text.pass, text.restart);
    } else {
      ShowError(response);
    }
  } else if (menu.page == Page::SaveList) {
    if (menu.saves[menu.index].active) {
      Popup(text.saves, text.active);
      return;
    }
    response = AdminRequest("POST", "/memorial/v1/slots/" +
                                        std::to_string(menu.saves[menu.index].usn) + "/select");
    if (response.ok()) {
      ShowRestartPage(Action::BackSaves);
      Popup(text.saves, text.restart);
    } else {
      ShowError(response);
    }
  }
}

std::string JsonEscape(const std::string_view input) {
  std::string output;
  output.reserve(input.size());
  for (const char value : input) {
    if (value == '\\' || value == '"') output.push_back('\\');
    output.push_back(value);
  }
  return output;
}

void CreateSave() {
  const auto& text = Text(menu.manager);
  const auto response = AdminRequest(
      "POST", "/memorial/v1/slots",
      "{\"displayName\":\"Memorial Save " + std::to_string(menu.saves.size() + 1) + "\"}");
  if (!response.ok()) {
    ShowError(response);
    return;
  }
  Popup(text.saves, text.sent);
  menu.index = 0;
  ShowSaveList();
}

void DeleteCurrentSave() {
  if (menu.saves.empty()) return;
  const auto& text = Text(menu.manager);
  const auto& slot = menu.saves[menu.index];
  const auto body = "{\"confirmDisplayName\":\"" + JsonEscape(slot.name) + "\"}";
  const auto response = AdminRequest(
      "DELETE", "/memorial/v1/slots/" + std::to_string(slot.usn), body);
  if (!response.ok()) {
    ShowError(response);
    return;
  }
  Popup(text.saves, text.sent);
  menu.index = 0;
  ShowSaveList(true);
}

void SendAmount(const std::string_view amount) {
  const auto& text = Text(menu.manager);
  const auto body = std::string("{\"amount\":\"") + std::string(amount) + "\"}";
  const auto response = AdminRequest("POST", "/memorial/v1/currency/" +
                                                menu.selected_currency + "/mail", body);
  if (response.ok()) pending_mail_refresh = true;
  ShowCurrencyList();
  response.ok() ? Popup(text.currency, text.sent) : ShowError(response);
}

void CloseSettings() {
  currency_amount.Publish();
  DismissMemorialPage();
  if (!ResolveTypes()) return;
  const auto* close = class_get_method(setting_class, "OnUIEventClose", 0);
  Invoke(close, menu.manager, nullptr);
  menu.page = Page::Normal;
}

void Dispatch(const int row) {
  if (row < 0 || row >= 4) return;
  switch (menu.actions[row]) {
    case Action::Enter: ShowRoot(); break;
    case Action::Close: CloseSettings(); break;
    case Action::BackRoot: ShowRoot(); break;
    case Action::BackMore: ShowMore(); break;
    case Action::Legacy: menu.index = 0; ShowLegacyArea(); break;
    case Action::Currency: menu.index = 0; ShowCurrencyList(); break;
    case Action::More: ShowMore(); break;
    case Action::Pass: menu.index = 0; menu.seasons.clear(); ShowPassList(); break;
    case Action::Saves: menu.index = 0; ShowSaveRoot(); break;
    case Action::SaveList: menu.index = 0; ShowSaveList(); break;
    case Action::SaveCreate: CreateSave(); break;
    case Action::SaveDelete: menu.index = 0; ShowSaveList(true); break;
    case Action::DeleteCurrent: DeleteCurrentSave(); break;
    case Action::BackSaves: ShowSaveRoot(); break;
    case Action::About:
      Popup(Text(menu.manager).menu, Text(menu.manager).about_description);
      break;
    case Action::Area1: menu.area = 1; ShowLegacyKind(); break;
    case Action::Area2: menu.area = 2; ShowLegacyKind(); break;
    case Action::Costume: menu.kind = "costume"; menu.index = 0; ShowLegacyList(); break;
    case Action::Guitar: menu.kind = "guitar"; menu.index = 0; ShowLegacyList(); break;
    case Action::Music: menu.kind = "music"; menu.index = 0; ShowLegacyList(); break;
    case Action::Previous: Move(-1); break;
    case Action::Next: Move(1); break;
    case Action::Select: SelectCurrent(); break;
    case Action::Amount100: SendAmount("100"); break;
    case Action::Amount1K: SendAmount("1K"); break;
    case Action::Amount1A: SendAmount("1A"); break;
    case Action::Restart: QuitGame(); break;
    case Action::None: break;
  }
}

void ShowHook(void* manager, const void* method) {
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "ui: settings Show manager=%p api=%d", manager, ApiReady());
  original_show(manager, method);
  menu = {};
  legacy_selection.Publish();
  menu.manager = manager;
  currency_amount.Publish();
  menu.actions[0] = Action::Enter;
  PrepareMemorialRows(manager);
  SetRow(manager, AuthObjects(), Text(manager).menu);
  TraceRow(ObjectAt(manager, AuthObjects()[0]), "auth");
  TraceRow(ObjectAt(manager, RestoreObjects()[0]), "restore");
}

void EnableHook(void* manager, const void* method) {
  original_enable(manager, method);
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "ui: settings enabled manager=%p", manager);
  PrepareMemorialRows(manager);
  SetRow(manager, AuthObjects(), Text(manager).menu);
}

void RefreshHook(void* manager, const void* method) {
  original_refresh(manager, method);
  // The final stock refresh also toggles the platform containers. Apply the
  // fixed entry AFTER all stock visibility/localization work, not only Show.
  PrepareMemorialRows(manager);
  SetRow(manager, AuthObjects(), Text(manager).menu);
}

void MailboxStartHook(void* manager, const void* method) {
  if (mailbox_handle != 0) {
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "ui: mailbox releasing previous weak handle=0x%" PRIxPTR,
                        mailbox_handle);
    handle_free(mailbox_handle);
    mailbox_handle = 0;
  }
  mailbox_handle = weak_handle_new(manager, false);
  __android_log_print(ANDROID_LOG_INFO, "GGFM",
                      "ui: mailbox Start manager=%p weak handle=0x%" PRIxPTR,
                      manager, mailbox_handle);
  original_mailbox_start(manager, method);
}

void DisableHook(void* manager, const void* method) {
  legacy_selection.Publish();
  currency_amount.Publish();
  DismissMemorialPage();
  original_disable(manager, method);
  // Match the tested loader: the mailbox caches its list, so an admin mail
  // must trigger the stock getPostTime chain when Settings closes. This runs
  // on Unity's thread. A weak handle avoids retaining a destroyed scene.
  if (!pending_mail_refresh || mailbox_handle == 0) return;
  void* mailbox = handle_target(mailbox_handle);
  auto* unity_object = FindClass("UnityEngine", "Object");
  if (mailbox == nullptr || unity_object == nullptr) return;
  const auto* alive = class_get_method(unity_object, "op_Implicit", 1);
  void* arguments[] = {mailbox};
  auto* boxed_alive = Invoke(alive, nullptr, arguments);
  if (boxed_alive == nullptr) return;
  const auto* is_alive = static_cast<bool*>(object_unbox(boxed_alive));
  if (is_alive == nullptr || !*is_alive) return;
  pending_mail_refresh = false;
  __android_log_print(ANDROID_LOG_INFO, "GGFM", "ui: refreshing stock mailbox after memorial delivery");
  request_post_time(mailbox, nullptr);
}

void AuthVisibilityHook(void* manager, const void* method) {
  original_auth_visibility(manager, method);
  // The stock refresh first hides all four Android login/logout objects and
  // then selects one from retired publisher state. Keep the memorial entry
  // stable even when that refresh runs after Show() or after account checks.
  PrepareMemorialRows(manager);
  if (menu.manager == manager) {
    SetRow(manager, AuthObjects(), Text(manager).menu);
  }
}

void LoginHook(void* manager, const void* method) {
  if (menu.manager == manager && menu.page == Page::LegacyList) {
    legacy_selection.Publish();
    ShowLegacyKind();
  } else if (menu.manager == manager) Dispatch(0);
  else original_login(manager, method);
}
void LogoutHook(void* manager, const void* method) {
  if (menu.manager == manager) Dispatch(0);
  else original_logout(manager, method);
}
void RestoreHook(void* manager, const void* method) {
  if (menu.manager == manager && menu.page != Page::Normal) Dispatch(1);
  else original_restore(manager, method);
}
void HelpHook(void* manager, const void* method) {
  if (const auto input = currency_amount.Consume()) {
    if (menu.manager == manager && menu.page == Page::CurrencyAmount && currency_amount.Current(*input)) {
      SendAmount(input->amount);
    }
    return;
  }
  if (const auto selected = legacy_selection.Consume()) {
    const bool valid_item = (menu.page == Page::LegacyList && selected->index < menu.legacy.size()) ||
                           (menu.page == Page::CurrencyList && selected->index < menu.currencies.size()) ||
                           (menu.page == Page::PassList && selected->index < menu.seasons.size());
    if (menu.manager == manager && valid_item && legacy_selection.Current(*selected)) {
      menu.index = selected->index;
      SelectCurrent();
    }
    return;  // Stale list events must not become an unrelated menu action.
  }
  if (menu.manager == manager && menu.page != Page::Normal) Dispatch(2);
  else original_help(manager, method);
}
void PrivacyHook(void* manager, const void* method) {
  if (menu.manager == manager && menu.page != Page::Normal) Dispatch(3);
  else original_privacy(manager, method);
}
}  // namespace

bool QueueLegacySelection(std::uint32_t generation, int index) {
  return legacy_selection.Queue(generation, index);
}

bool QueueCurrencyAmount(std::uint32_t generation, std::string_view amount) {
  return currency_amount.Queue(generation, amount);
}

bool InitializeMemorialUiApi(void* il2cpp_handle, const std::uintptr_t il2cpp_base) {
  // Unity loads IL2CPP locally. RTLD_DEFAULT cannot resolve its exports on the
  // standalone loader; use the same verified library handle as runtime Hooks.
  void* scope = il2cpp_handle == nullptr ? RTLD_DEFAULT : il2cpp_handle;
  string_new = Symbol<StringNew>(scope, "il2cpp_string_new");
  domain_get = Symbol<DomainGet>(scope, "il2cpp_domain_get");
  domain_assemblies = Symbol<DomainAssemblies>(scope, "il2cpp_domain_get_assemblies");
  assembly_image = Symbol<AssemblyImage>(scope, "il2cpp_assembly_get_image");
  class_from_name = Symbol<ClassFromName>(scope, "il2cpp_class_from_name");
  class_get_method = Symbol<ClassGetMethod>(scope, "il2cpp_class_get_method_from_name");
  class_get_type = Symbol<ClassGetType>(scope, "il2cpp_class_get_type");
  type_get_object = Symbol<TypeGetObject>(scope, "il2cpp_type_get_object");
  runtime_invoke = Symbol<RuntimeInvoke>(scope, "il2cpp_runtime_invoke");
  weak_handle_new = Symbol<WeakHandleNew>(scope, "il2cpp_gchandle_new_weakref");
  handle_target = Symbol<HandleTarget>(scope, "il2cpp_gchandle_get_target");
  handle_free = Symbol<HandleFree>(scope, "il2cpp_gchandle_free");
  object_unbox = Symbol<ObjectUnbox>(scope, "il2cpp_object_unbox");
  class_get_field = Symbol<ClassGetField>(scope, "il2cpp_class_get_field_from_name");
  field_set_value = Symbol<FieldSetValue>(scope, "il2cpp_field_set_value");
  for (const auto& dependency : kGeneratedDependencies) {
    if (dependency.name == "ui.mailbox.requestPostTime")
      request_post_time = reinterpret_cast<VoidInstance>(il2cpp_base + dependency.rva);
  }
  __android_log_print(ApiReady() ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "GGFM",
                      "ui: IL2CPP API ready=%d", ApiReady());
  return ApiReady();
}

HookBinding ResolveMemorialUiHook(const std::string_view name) {
  if (name == "ui.setting.refresh")
    return {reinterpret_cast<void*>(RefreshHook), reinterpret_cast<void**>(&original_refresh)};
  if (name == "ui.setting.enable")
    return {reinterpret_cast<void*>(EnableHook), reinterpret_cast<void**>(&original_enable)};
  if (name == "ui.setting.disable")
    return {reinterpret_cast<void*>(DisableHook), reinterpret_cast<void**>(&original_disable)};
  if (name == "ui.mailbox.start")
    return {reinterpret_cast<void*>(MailboxStartHook), reinterpret_cast<void**>(&original_mailbox_start)};
  if (name == "ui.setting.show")
    return {reinterpret_cast<void*>(ShowHook), reinterpret_cast<void**>(&original_show)};
  if (name == "ui.setting.authVisibility")
    return {reinterpret_cast<void*>(AuthVisibilityHook),
            reinterpret_cast<void**>(&original_auth_visibility)};
  if (name == "ui.setting.login")
    return {reinterpret_cast<void*>(LoginHook), reinterpret_cast<void**>(&original_login)};
  if (name == "ui.setting.logout")
    return {reinterpret_cast<void*>(LogoutHook), reinterpret_cast<void**>(&original_logout)};
  if (name == "ui.setting.restore")
    return {reinterpret_cast<void*>(RestoreHook), reinterpret_cast<void**>(&original_restore)};
  if (name == "ui.setting.help")
    return {reinterpret_cast<void*>(HelpHook), reinterpret_cast<void**>(&original_help)};
  if (name == "ui.setting.privacy")
    return {reinterpret_cast<void*>(PrivacyHook), reinterpret_cast<void**>(&original_privacy)};
  return {nullptr, nullptr};
}

}  // namespace ggfm
