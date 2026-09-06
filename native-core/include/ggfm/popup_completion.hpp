#pragma once
#include "ggfm/il2cpp_gc_handle.hpp"
#include <cstdint>

namespace ggfm {
// Unity dispatches UI events on its main thread. Claim the current popup before
// calling into stock code; later taps on the closing popup are not new purchases.
inline bool BeginPopupConfirmation(std::int32_t& state) {
  if (state != 2) return false;
  state = 3;
  return true;
}
// The original wait's final state only dispatches the mutable animator callback.
// EndScale now owns that dispatch. An old wait must never complete a new popup.
inline bool FinishPopupDelayedCallback(std::int32_t& state) {
  if (state != 2) return false;
  state = -1;
  return true;
}

struct PopupCompletionApi {
  Il2CppGcHandle (*retain)(void*, bool);
  HandleTarget target;
  HandleFree release;
  void (*clear)(void*, void**, void*);
  bool (*invoke)(void*);
};

inline bool CompletePopupOnce(void* animator, void** callback, bool closing,
                              const PopupCompletionApi& api) {
  if (closing || *callback == nullptr) return true;
  // Strongly root across clearing the field and managed re-entrancy. Never clear
  // after Invoke: a callback may synchronously open the same cached popup again.
  const auto handle = api.retain(*callback, false);
  if (handle == 0) return false;
  api.clear(animator, callback, nullptr);
  auto* action = api.target(handle);
  const bool ok = action != nullptr && api.invoke(action);
  api.release(handle);
  return ok;
}
}  // namespace ggfm
