#pragma once

#include <string_view>
#include <cstdint>

#include "ggfm/hook_backend.hpp"

namespace ggfm {

bool InitializeMemorialUiApi(void* il2cpp_handle, std::uintptr_t il2cpp_base);
HookBinding ResolveMemorialUiHook(std::string_view name);
bool QueueLegacySelection(std::uint32_t generation, int index);
bool QueueCurrencyAmount(std::uint32_t generation, std::string_view amount);

}  // namespace ggfm
