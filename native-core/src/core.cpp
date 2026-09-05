#include "ggfm/hook_backend.hpp"

#include <android/log.h>
#include <cinttypes>
#include <cstring>

namespace ggfm {

bool ValidateFingerprints(const std::uintptr_t il2cpp_base,
                          const std::span<const FingerprintTarget> targets) {
  if (il2cpp_base == 0) return false;
  for (const auto& target : targets) {
    const auto* address = reinterpret_cast<const void*>(il2cpp_base + target.rva);
    if (std::memcmp(address, target.expected_prologue.data(),
                    target.expected_prologue.size()) != 0) {
      return false;
    }
  }
  return true;
}

bool InstallHooks(HookBackend& backend, const std::uintptr_t il2cpp_base,
                  const std::span<const HookTarget> targets,
                  const ResolveReplacement resolve_replacement) {
  if (il2cpp_base == 0 || resolve_replacement == nullptr) return false;
  // Validate the complete version fingerprint before changing one instruction.
  for (const auto& target : targets) {
    const FingerprintTarget fingerprint{target.name, target.rva,
                                        target.expected_prologue};
    if (!ValidateFingerprints(il2cpp_base,
                              std::span<const FingerprintTarget>(&fingerprint, 1))) {
      __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                          "runtime: fingerprint mismatch for %.*s at RVA 0x%zx",
                          static_cast<int>(target.name.size()), target.name.data(),
                          static_cast<std::size_t>(target.rva));
      return false;
    }
    if (resolve_replacement(target.name).replacement == nullptr) {
      __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                          "runtime: no replacement registered for %.*s",
                          static_cast<int>(target.name.size()), target.name.data());
      return false;
    }
  }
  for (const auto& target : targets) {
    auto* address = reinterpret_cast<void*>(il2cpp_base + target.rva);
    const auto binding = resolve_replacement(target.name);
    std::uint8_t before[16]{};
    std::memcpy(before, address, sizeof(before));
    if (!backend.Install(address, binding.replacement, binding.original)) {
      __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                          "runtime: Dobby failed for %.*s",
                          static_cast<int>(target.name.size()), target.name.data());
      return false;
    }
    std::uint8_t after[16]{};
    std::memcpy(after, address, sizeof(after));
    if (std::memcmp(before, after, sizeof(before)) == 0) {
      __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                          "runtime: Hook did not alter %.*s at 0x%" PRIxPTR,
                          static_cast<int>(target.name.size()), target.name.data(),
                          reinterpret_cast<std::uintptr_t>(address));
      return false;
    }
    __android_log_print(ANDROID_LOG_INFO, "GGFM",
                        "runtime: Hook active for %.*s at 0x%" PRIxPTR,
                        static_cast<int>(target.name.size()), target.name.data(),
                        reinterpret_cast<std::uintptr_t>(address));
  }
  return true;
}

}  // namespace ggfm
