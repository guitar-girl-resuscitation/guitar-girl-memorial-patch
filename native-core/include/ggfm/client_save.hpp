#pragma once

#include "ggfm/hook_backend.hpp"

namespace ggfm {
// Preserve the game's success/failure result and do not save rejected actions.
template <class Operation, class QueueSave>
bool SaveAfterLocalSuccess(Operation operation, QueueSave queue_save) {
  const bool success = operation();
  if (success) queue_save();
  return success;
}
bool InitializeClientSave(std::uintptr_t il2cpp_base);
HookBinding ResolveClientSaveHook(std::string_view name);
}
