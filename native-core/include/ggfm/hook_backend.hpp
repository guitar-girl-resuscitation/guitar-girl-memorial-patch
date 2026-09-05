#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <span>
#include <string_view>

namespace ggfm {

struct HookTarget {
  std::string_view name;
  std::uintptr_t rva;
  std::array<std::uint8_t, 16> expected_prologue;
};

struct FingerprintTarget {
  std::string_view name;
  std::uintptr_t rva;
  std::array<std::uint8_t, 16> expected_prologue;
};

struct GeneratedFieldOffset {
  std::string_view name;
  std::uintptr_t offset;
};

class HookBackend {
 public:
  virtual ~HookBackend() = default;
  virtual bool Install(void* target, void* replacement, void** original) = 0;
};

struct HookBinding {
  void* replacement;
  void** original;
};

using ResolveReplacement = HookBinding (*)(std::string_view name);
bool InstallHooks(HookBackend& backend, std::uintptr_t il2cpp_base,
                  std::span<const HookTarget> targets,
                  ResolveReplacement resolve_replacement);
bool ValidateFingerprints(std::uintptr_t il2cpp_base,
                          std::span<const FingerprintTarget> targets);

}  // namespace ggfm
