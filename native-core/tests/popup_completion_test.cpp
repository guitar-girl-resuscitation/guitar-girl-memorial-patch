#include "ggfm/popup_completion.hpp"
#include <cassert>
#include <initializer_list>

namespace {
int first, second, calls, roots;
void* slot;
bool reenter, fail;
ggfm::Il2CppGcHandle Retain(void* p, bool pinned) {
  assert(!pinned); ++roots;
  return reinterpret_cast<ggfm::Il2CppGcHandle>(p);
}
void* Target(ggfm::Il2CppGcHandle h) { return reinterpret_cast<void*>(h); }
void Release(ggfm::Il2CppGcHandle) { --roots; }
void Clear(void*, void** p, void* v) { assert(roots == 1); *p = v; }
bool Invoke(void* p) {
  assert(roots == 1 && slot == nullptr);
  assert(p == &first || p == &second);
  ++calls;
  if (reenter) slot = &second;
  return !fail;
}
}
int main() {
  for (std::int32_t initial : {0, 1, 3, 4}) {
    auto state = initial;
    assert(!ggfm::BeginPopupConfirmation(state) && state == initial);
  }
  std::int32_t popup = 2;
  assert(ggfm::BeginPopupConfirmation(popup) && popup == 3);
  assert(!ggfm::BeginPopupConfirmation(popup));
  popup = 2; // a different/new opening is not blocked by the previous click
  assert(ggfm::BeginPopupConfirmation(popup));
  const ggfm::PopupCompletionApi api{Retain, Target, Release, Clear, Invoke};
  slot = &first;
  assert(ggfm::CompletePopupOnce(nullptr, &slot, true, api));
  assert(calls == 0 && slot == &first); // closing never grants/open-completes
  assert(ggfm::CompletePopupOnce(nullptr, &slot, false, api));
  assert(calls == 1 && !slot && !roots);
  assert(ggfm::CompletePopupOnce(nullptr, &slot, false, api));
  assert(calls == 1); // repeated tween completion is harmless
  slot = &first; reenter = true;
  assert(ggfm::CompletePopupOnce(nullptr, &slot, false, api));
  assert(slot == &second && !roots); // a newly opened popup survives
  std::int32_t old_wait = 2;
  assert(ggfm::FinishPopupDelayedCallback(old_wait));
  assert(old_wait == -1 && slot == &second && calls == 2);
  reenter = false; fail = true;
  assert(!ggfm::CompletePopupOnce(nullptr, &slot, false, api));
  assert(!slot && !roots); // invocation failure does not leak roots
  for (std::int32_t state : {-1, 0, 1}) {
    const auto before = state;
    assert(!ggfm::FinishPopupDelayedCallback(state) && state == before);
  }
}
