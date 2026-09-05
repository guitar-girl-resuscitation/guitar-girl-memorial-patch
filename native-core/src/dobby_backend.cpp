#include "ggfm/hook_backend.hpp"

extern "C" int DobbyHook(void* address, void* replacement, void** original);

namespace ggfm {
class DobbyBackend final : public HookBackend {
 public:
  bool Install(void* target, void* replacement, void** original) override {
    return DobbyHook(target, replacement, original) == 0;
  }
};
}  // namespace ggfm
