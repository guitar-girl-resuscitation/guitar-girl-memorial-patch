#pragma once

#include "ggfm/hook_backend.hpp"

namespace ggfm {
bool InitializeGameplayCompatibility(std::uintptr_t il2cpp_base);
HookBinding ResolveGameplayCompatibilityHook(std::string_view name);
}
