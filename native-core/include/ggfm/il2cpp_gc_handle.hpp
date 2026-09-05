#pragma once

#include <cstdint>

namespace ggfm {
// The supported 8.0.0 ARM64 runtime returns a pointer-sized handle in x0.
// Its free/get-target exports consume that full value, not a uint32 index.
// Keep the ABI shared by storage and all three dynamically loaded functions.
using Il2CppGcHandle = std::uintptr_t;
using WeakHandleNew = Il2CppGcHandle (*)(void*, bool);
using HandleTarget = void* (*)(Il2CppGcHandle);
using HandleFree = void (*)(Il2CppGcHandle);
static_assert(sizeof(Il2CppGcHandle) == sizeof(void*));
}  // namespace ggfm
