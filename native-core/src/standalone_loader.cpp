#include "ggfm/runtime_hooks.hpp"
#include "ggfm/loopback_boundary.hpp"

#include <android/log.h>
#include <dlfcn.h>
#include <link.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <atomic>
#include <cinttypes>
#include <cstdlib>
#include <cstring>

extern "C" int DobbyHook(void* address, void* replacement, void** original);
extern "C" int DobbyDestroy(void* address);

namespace {
using Connect = int (*)(int, const sockaddr*, socklen_t);
using Dlopen = void* (*)(const char*, int);
Connect original_connect = nullptr;
Dlopen original_dlopen = nullptr;
void* dlopen_target = nullptr;
std::atomic<bool> installed{false};
std::atomic<bool> network_boundary_installed{false};
std::atomic<bool> runtime_observer_installed{false};
std::atomic<bool> runtime_installing{false};
std::atomic<bool> firebase_only_profile{false};
std::atomic<bool> retired_native_sdks_installed{false};

void Log(const int priority, const char* message) {
  __android_log_print(priority, "GGFM", "%s", message);
}

class Backend final : public ggfm::HookBackend {
 public:
  bool Install(void* target, void* replacement, void** original) override {
    // Dobby's Android ARM64 implementation unconditionally writes the
    // generated trampoline through the third argument. Some GGFM replacements
    // intentionally never call the original, but the output slot must still
    // be valid for the duration of DobbyHook.
    void* ignored_trampoline = nullptr;
    return DobbyHook(target, replacement,
                     original == nullptr ? &ignored_trampoline : original) == 0;
  }
};

// Firebase C++ registers its Messaging initializer independently from the
// game's managed FirebaseManagerSGT. Disabling the retired Java provider is
// therefore not enough: App::Create later invokes this exported function and
// Android 16 aborts when the SDK calls through a null Java peer. The memorial
// build has no remote-push backend, so report Firebase's success value (zero)
// without starting Messaging. The two-argument overload is a single ARM64
// branch into this options overload, so intercepting the implementation covers
// both without overwriting the adjacent four-byte thunk.
int FirebaseMessagingInitializeWithOptionsHook(void*, void*, const void*) {
  Log(ANDROID_LOG_INFO,
      "runtime: retired Firebase Messaging initializer satisfied locally");
  return 0;
}

void* FirebaseCrashlyticsGetInstanceHook(void*, std::int32_t* init_result) {
  // SWIG treats a null native pointer as an unavailable optional service. The
  // managed Crashlytics wrapper already guards this state; no gameplay data is
  // carried by the retired crash reporter.
  if (init_result != nullptr) *init_result = 0;
  Log(ANDROID_LOG_INFO,
      "runtime: retired Firebase Crashlytics instance suppressed locally");
  return nullptr;
}

bool InstallRetiredNativeSdkHooks() {
  if (retired_native_sdks_installed.load(std::memory_order_acquire)) return true;
  void* firebase = dlopen("libFirebaseCppApp-13_4_0.so", RTLD_NOW | RTLD_NOLOAD);
  if (firebase == nullptr) {
    Log(ANDROID_LOG_ERROR,
        "runtime: preloaded Firebase C++ library is unavailable");
    return false;
  }
  constexpr const char* kInitializeWithOptions =
      "_ZN8firebase9messaging10InitializeERKNS_3AppEPNS0_8ListenerERKNS0_16MessagingOptionsE";
  constexpr const char* kCrashlyticsGetInstance =
      "_ZN8firebase11crashlytics11Crashlytics11GetInstanceEPNS_3AppEPNS_10InitResultE";
  constexpr const char* kSetAllAppCallbacks =
      "Firebase_App_CSharp_SetEnabledAllAppCallbacks";
  void* initialize_with_options = dlsym(firebase, kInitializeWithOptions);
  void* crashlytics_get_instance = dlsym(firebase, kCrashlyticsGetInstance);
  auto set_all_app_callbacks = reinterpret_cast<void (*)(bool)>(
      dlsym(firebase, kSetAllAppCallbacks));
  void* ignored_initialize_with_options = nullptr;
  void* ignored_crashlytics_get_instance = nullptr;
  if (set_all_app_callbacks != nullptr) set_all_app_callbacks(false);
  const bool hooked =
      set_all_app_callbacks != nullptr && initialize_with_options != nullptr &&
      crashlytics_get_instance != nullptr &&
      DobbyHook(initialize_with_options,
                reinterpret_cast<void*>(FirebaseMessagingInitializeWithOptionsHook),
                &ignored_initialize_with_options) == 0 &&
      DobbyHook(crashlytics_get_instance,
                reinterpret_cast<void*>(FirebaseCrashlyticsGetInstanceHook),
                &ignored_crashlytics_get_instance) == 0;
  dlclose(firebase);
  if (!hooked) {
    Log(ANDROID_LOG_ERROR,
        "runtime: retired Firebase Messaging native Hook failed");
    return false;
  }
  retired_native_sdks_installed.store(true, std::memory_order_release);
  Log(ANDROID_LOG_INFO,
      "runtime: retired native SDK compatibility Hooks installed");
  return true;
}

std::uintptr_t FindIl2Cpp() {
  std::uintptr_t result = 0;
  dl_iterate_phdr(
      [](dl_phdr_info* info, std::size_t, void* data) {
        if (info->dlpi_name != nullptr &&
            std::strstr(info->dlpi_name, "libil2cpp.so") != nullptr) {
          *static_cast<std::uintptr_t*>(data) = info->dlpi_addr;
          return 1;
        }
        return 0;
      },
      &result);
  return result;
}

void LogIl2CppMappings() {
  int count = 0;
  dl_iterate_phdr(
      [](dl_phdr_info* info, std::size_t, void* data) {
        if (info->dlpi_name != nullptr &&
            std::strstr(info->dlpi_name, "libil2cpp.so") != nullptr) {
          auto* mapping_count = static_cast<int*>(data);
          ++*mapping_count;
          __android_log_print(ANDROID_LOG_INFO, "GGFM",
                              "runtime: IL2CPP mapping #%d base=0x%" PRIxPTR " path=%s",
                              *mapping_count,
                              static_cast<std::uintptr_t>(info->dlpi_addr),
                              info->dlpi_name);
        }
        return 0;
      },
      &count);
  if (count == 0) Log(ANDROID_LOG_ERROR, "runtime: no IL2CPP mapping is loaded");
}

bool InstallFromLoadedHandle(void* il2cpp_handle,
                             const bool firebase_only_diagnostic) {
  if (installed.load(std::memory_order_acquire)) return true;
  if (il2cpp_handle == nullptr) return false;
  bool expected = false;
  if (!runtime_installing.compare_exchange_strong(
          expected, true, std::memory_order_acq_rel)) {
    return installed.load(std::memory_order_acquire);
  }
  LogIl2CppMappings();
  const auto base = FindIl2Cpp();
  if (base == 0) {
    runtime_installing.store(false, std::memory_order_release);
    return false;
  }
  Backend backend;
  const bool hooked = ggfm::InstallRuntimeHooks(
      backend, base, il2cpp_handle, firebase_only_diagnostic);
  if (!hooked) {
    runtime_installing.store(false, std::memory_order_release);
    Log(ANDROID_LOG_ERROR, "runtime: IL2CPP fingerprint or Hook installation failed");
    return false;
  }
  installed.store(true, std::memory_order_release);
  runtime_installing.store(false, std::memory_order_release);
  Log(ANDROID_LOG_INFO, "runtime: IL2CPP loaded and all Hooks installed");
  return true;
}

bool InstallIfLoaded(const bool firebase_only_diagnostic) {
  if (installed.load(std::memory_order_acquire)) return true;
  void* il2cpp_handle = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
  if (il2cpp_handle == nullptr) {
    const char* error = dlerror();
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "runtime: cannot acquire loaded IL2CPP handle: %s",
                        error == nullptr ? "unknown linker error" : error);
    return false;
  }
  const bool hooked = InstallFromLoadedHandle(il2cpp_handle,
                                               firebase_only_diagnostic);
  dlclose(il2cpp_handle);
  return hooked;
}

void* DlopenHook(const char* filename, const int flags) {
  void* handle = original_dlopen(filename, flags);
  if (handle != nullptr && filename != nullptr &&
      std::strstr(filename, "libil2cpp.so") != nullptr) {
    Log(ANDROID_LOG_INFO,
        "runtime: observed Unity dlopen for IL2CPP; installing Hooks now");
    if (!InstallFromLoadedHandle(
            handle, firebase_only_profile.load(std::memory_order_acquire))) {
      Log(ANDROID_LOG_FATAL,
          "runtime: failed to install Hooks on Unity dlopen mapping");
    }
    // bionic selects a linker namespace from dlopen's caller address. The
    // first observed call has already completed; restore dlopen immediately
    // so Unity's later EGL and graphics loads retain their real caller.
    if (dlopen_target == nullptr || DobbyDestroy(dlopen_target) != 0) {
      Log(ANDROID_LOG_FATAL,
          "runtime: failed to restore dlopen after IL2CPP observation");
    } else {
      Log(ANDROID_LOG_INFO,
          "runtime: IL2CPP observer consumed; system dlopen restored");
    }
  }
  return handle;
}

bool InstallRuntimeLoadObserver(const bool firebase_only_diagnostic) {
  if (runtime_observer_installed.load(std::memory_order_acquire)) return true;
  firebase_only_profile.store(firebase_only_diagnostic,
                              std::memory_order_release);
  dlopen_target = dlsym(RTLD_DEFAULT, "dlopen");
  if (dlopen_target == nullptr ||
      DobbyHook(dlopen_target, reinterpret_cast<void*>(DlopenHook),
                reinterpret_cast<void**>(&original_dlopen)) != 0 ||
      original_dlopen == nullptr) {
    return false;
  }
  runtime_observer_installed.store(true, std::memory_order_release);
  Log(ANDROID_LOG_INFO,
      "runtime: one-shot Unity IL2CPP load observer installed");
  return true;
}

int LoopbackOnlyConnect(const int socket, const sockaddr* address,
                        const socklen_t length) {
  if (address == nullptr) return original_connect(socket, address, length);
  if (!ggfm::IsLocalConnectAddress(address, length)) {
    static std::atomic<unsigned> denied{0};
    if (denied.fetch_add(1, std::memory_order_relaxed) < 8)
      Log(ANDROID_LOG_WARN, "network: denied non-loopback or malformed connect");
    errno = EACCES;
    return -1;
  }
  return original_connect(socket, address, length);
}

bool InstallNetworkBoundary() {
  if (network_boundary_installed.load(std::memory_order_acquire)) return true;
  void* target = dlsym(RTLD_DEFAULT, "connect");
  if (target == nullptr ||
      DobbyHook(target, reinterpret_cast<void*>(LoopbackOnlyConnect),
                reinterpret_cast<void**>(&original_connect)) != 0) {
    return false;
  }
  network_boundary_installed.store(true, std::memory_order_release);
  return true;
}
}  // namespace

extern "C" bool ggfm_loader_prepare() {
  if (!InstallNetworkBoundary()) {
    Log(ANDROID_LOG_ERROR, "runtime: loopback-only network boundary failed");
    return false;
  }
  Log(ANDROID_LOG_INFO, "runtime: loopback-only network boundary installed");
  return true;
}

extern "C" bool ggfm_loader_install_retired_native_sdks() {
  return InstallRetiredNativeSdkHooks();
}

extern "C" bool ggfm_loader_install_runtime(const bool firebase_only_diagnostic) {
  // Java has already loaded libil2cpp through the application's ClassLoader on
  // the UI thread. Never create a second linker-namespace mapping here.
  return InstallIfLoaded(firebase_only_diagnostic);
}

extern "C" bool ggfm_loader_arm_runtime(const bool firebase_only_diagnostic) {
  // Unity must own libil2cpp loading. Loading it from the bootstrap activity
  // creates a valid mapping but leaves Unity's runtime initialization stuck.
  // Observe Android's real load instead and install the verified inline Hooks
  // before android_dlopen_ext returns to Unity.
  return InstallRuntimeLoadObserver(firebase_only_diagnostic);
}
