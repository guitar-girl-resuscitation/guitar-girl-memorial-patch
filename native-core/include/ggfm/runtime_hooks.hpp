#pragma once

#include <cstdint>

#include "ggfm/hook_backend.hpp"

namespace ggfm {
bool InstallRuntimeHooks(HookBackend& backend, std::uintptr_t il2cpp_base,
                         void* il2cpp_handle = nullptr,
                         bool firebase_only_diagnostic = false);
}
