#include "ggfm/hook_backend.hpp"
#include "ggfm/runtime_hooks.hpp"

namespace ggfm {
class LsposedBackend final : public HookBackend {
 public:
  using HookFunction = int (*)(void*, void*, void**);
  explicit LsposedBackend(HookFunction hook) : hook_(hook) {}
  bool Install(void* target, void* replacement, void** original) override {
    return hook_ != nullptr && hook_(target, replacement, original) == 0;
  }

 private:
  HookFunction hook_;
};
}  // namespace ggfm

extern "C" bool ggfm_lsposed_install(void* hook_function,
                                      const std::uintptr_t il2cpp_base) {
  auto hook = reinterpret_cast<ggfm::LsposedBackend::HookFunction>(hook_function);
  ggfm::LsposedBackend backend(hook);
  return ggfm::InstallRuntimeHooks(backend, il2cpp_base);
}
