#include "ggfm/admin_bridge.hpp"
#include "ggfm/memorial_ui.hpp"
#include "ggfm/runtime.hpp"

#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>

#include <cstdint>
#include <cstring>
#include <string>

extern "C" {
std::uint32_t ggfm_server_abi_version();
const char* ggfm_server_policy_sha256();
std::int32_t ggfm_server_start(const char*, void*, std::int64_t, std::int32_t,
                               const char*);
const char* ggfm_server_endpoint();
std::int32_t ggfm_server_begin_login(std::int64_t, std::int32_t);
std::int64_t ggfm_server_active_usn();
std::size_t ggfm_server_drain_logs(char*, std::size_t);
std::size_t ggfm_server_startup_banner(std::uint32_t, char*, std::size_t);
std::int32_t ggfm_server_transfer_save(const char*, const char*, bool);
std::int32_t ggfm_server_prepare_shutdown();
std::int32_t ggfm_server_stop();
bool ggfm_loader_prepare();
bool ggfm_loader_install_retired_native_sdks();
bool ggfm_loader_install_runtime(bool firebase_only_diagnostic);
bool ggfm_loader_arm_runtime(bool firebase_only_diagnostic);
}

namespace {
JavaVM* java_vm = nullptr;
std::string Utf8(JNIEnv* env, jstring value) {
  if (value == nullptr) return {};
  const char* raw = env->GetStringUTFChars(value, nullptr);
  std::string result(raw == nullptr ? "" : raw);
  if (raw != nullptr) env->ReleaseStringUTFChars(value, raw);
  return result;
}
}  // namespace

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  java_vm = vm;
  return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_queueLegacySelection(
    JNIEnv*, jclass, jlong generation, jint index) {
  if (generation <= 0 || generation > UINT32_MAX) return JNI_FALSE;
  return ggfm::QueueLegacySelection(static_cast<std::uint32_t>(generation), index);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_queueCurrencyAmount(
    JNIEnv* env, jclass, jlong generation, jstring amount) {
  if (generation <= 0 || generation > UINT32_MAX || amount == nullptr ||
      env->GetStringLength(amount) > 64) return JNI_FALSE;
  return ggfm::QueueCurrencyAmount(static_cast<std::uint32_t>(generation), Utf8(env, amount));
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_startServer(
    JNIEnv* env, jclass, jstring data_dir, jobject assets,
    jlong unix_seconds, jint utc_offset_minutes, jstring capability) {
  if (ggfm_server_abi_version() != 1) return -10;
  const auto* server_policy = ggfm_server_policy_sha256();
  if (server_policy == nullptr ||
      std::strcmp(server_policy, GGFM_POLICY_SHA256) != 0) {
    __android_log_print(ANDROID_LOG_ERROR, "GGFM",
                        "policy mismatch: patch=%s server=%s; rebuild matching artifacts",
                        GGFM_POLICY_SHA256, server_policy == nullptr ? "missing" : server_policy);
    return -13;
  }
  if (java_vm == nullptr || !ggfm::InitializeAdminBridge(java_vm, env)) return -12;
  const auto directory = Utf8(env, data_dir);
  const auto session_capability = Utf8(env, capability);
  auto* manager = AAssetManager_fromJava(env, assets);
  const auto started = ggfm_server_start(directory.c_str(), manager, unix_seconds,
                                         utc_offset_minutes,
                                         session_capability.c_str());
  if (started != 0 && started != 1) return started;
  const auto login = ggfm_server_begin_login(unix_seconds, utc_offset_minutes);
  if (login != 0) return login;
  const auto usn = ggfm_server_active_usn();
  if (usn <= 0) return -14;
  ggfm::SetRequestContext(
      {ggfm_server_endpoint(), session_capability, usn, utc_offset_minutes});
  if (!ggfm::SetActiveUsnOnce(usn)) return -15;
  if (!ggfm_loader_prepare()) return -11;
  return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_installRuntimeHooks(
    JNIEnv*, jclass, jboolean firebase_only_diagnostic) {
  return ggfm_loader_install_runtime(firebase_only_diagnostic == JNI_TRUE) ? 0 : -11;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_installRetiredNativeSdkHooks(
    JNIEnv*, jclass) {
  return ggfm_loader_install_retired_native_sdks() ? 0 : -11;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_armRuntimeHooks(
    JNIEnv*, jclass, jboolean firebase_only_diagnostic) {
  return ggfm_loader_arm_runtime(firebase_only_diagnostic == JNI_TRUE) ? 0 : -11;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_endpoint(
    JNIEnv* env, jclass) {
  const auto context = ggfm::GetRequestContext();
  return env->NewStringUTF(context.endpoint.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_startupBanner(
    JNIEnv* env, jclass, jint columns) {
  char buffer[8 * 1024]{};
  const auto required = ggfm_server_startup_banner(
      columns > 0 ? static_cast<std::uint32_t>(columns) : 0, buffer, sizeof(buffer));
  if (required > sizeof(buffer)) return env->NewStringUTF("AIRISUTEK\n");
  return env->NewStringUTF(buffer);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_transferSave(
    JNIEnv* env, jclass, jstring directory, jstring staging, jboolean importing) {
  const auto dir = Utf8(env, directory);
  const auto path = Utf8(env, staging);
  return ggfm_server_transfer_save(dir.c_str(), path.c_str(), importing == JNI_TRUE);
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_drainServerLogs(
    JNIEnv* env, jclass) {
  char buffer[16 * 1024]{};
  const auto size = ggfm_server_drain_logs(buffer, sizeof(buffer));
  if (size == 0) return env->NewStringUTF("");
  return env->NewStringUTF(buffer);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_prepareShutdown(
    JNIEnv*, jclass) {
  return ggfm_server_prepare_shutdown();
}

extern "C" JNIEXPORT jint JNICALL
Java_org_guitargirlresuscitation_memorial_MemorialNative_stop(
    JNIEnv*, jclass) {
  return ggfm_server_stop();
}
