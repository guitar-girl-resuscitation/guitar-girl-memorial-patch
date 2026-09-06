#include "ggfm/hook_backend.hpp"

extern "C" int DobbyHook(void* address, void* replacement, void** original);
extern "C" void dobby_enable_near_branch_trampoline();

namespace ggfm {
class DobbyBackend final : public HookBackend {
 public:
  bool Install(void* target, void* replacement, void** original) override {
    static const bool near_routing = [] { dobby_enable_near_branch_trampoline(); return true; }();
    (void)near_routing;
    void* ignored = nullptr;
    return DobbyHook(target, replacement, original ? original : &ignored) == 0;
  }
};
}  // namespace ggfm
