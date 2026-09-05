#include "ggfm/runtime.hpp"

#include <array>
#include <atomic>
#include <string_view>

namespace ggfm {
namespace {
std::atomic<std::int64_t> active_usn{0};
constexpr std::array<std::string_view, 8> kInstallKeys = {
    "Screenmanager Resolution Width", "Screenmanager Resolution Height",
    "Screenmanager Is Fullscreen mode", "UnityGraphicsQuality",
    "MasterVolume", "BgmVolume", "SfxVolume", "Language"};

bool IsInstallKey(const std::string_view key) {
  for (const auto candidate : kInstallKeys) {
    if (key == candidate) return true;
  }
  return false;
}
}  // namespace

bool SetActiveUsnOnce(const std::int64_t usn) {
  if (usn <= 0) return false;
  std::int64_t expected = 0;
  return active_usn.compare_exchange_strong(expected, usn,
                                             std::memory_order_acq_rel) ||
         expected == usn;
}

std::string NamespacePlayerPrefsKey(const std::string_view original_key) {
  const auto usn = active_usn.load(std::memory_order_acquire);
  if (usn <= 0 || IsInstallKey(original_key)) return std::string(original_key);

  // Progression keys in this client are not consistently prefixed. A key is
  // installation-wide only when it is explicitly audited above; everything
  // else fails closed into the active save slot.
  return "ggfm/usn/" + std::to_string(usn) + "/" +
         std::string(original_key);
}

std::u16string NamespacePlayerPrefsKey(const std::u16string_view original_key) {
  const auto usn = active_usn.load(std::memory_order_acquire);
  if (usn <= 0) return std::u16string(original_key);

  std::string ascii;
  ascii.reserve(original_key.size());
  bool ascii_only = true;
  for (const auto character : original_key) {
    if (character > 0x7f) {
      ascii_only = false;
      break;
    }
    ascii.push_back(static_cast<char>(character));
  }
  if (ascii_only && IsInstallKey(ascii)) return std::u16string(original_key);

  const auto prefix_ascii = "ggfm/usn/" + std::to_string(usn) + "/";
  std::u16string result;
  result.reserve(prefix_ascii.size() + original_key.size());
  for (const auto character : prefix_ascii) {
    result.push_back(static_cast<char16_t>(character));
  }
  result.append(original_key);
  return result;
}

}  // namespace ggfm
