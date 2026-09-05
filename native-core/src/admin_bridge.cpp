#include "ggfm/admin_bridge.hpp"

#include "ggfm/runtime.hpp"

#include <charconv>
#include <mutex>

namespace ggfm {
namespace {
JavaVM* java_vm = nullptr;
jclass client_class = nullptr;
jmethodID request_method = nullptr;
jmethodID page_method = nullptr;
jmethodID message_method = nullptr;
jmethodID dismiss_method = nullptr;
jmethodID identity_method = nullptr;
jmethodID list_method = nullptr;
jmethodID currency_list_method = nullptr;
jmethodID currency_input_method = nullptr;
std::mutex bridge_mutex;

std::string Utf8(JNIEnv* env, jstring value) {
  if (value == nullptr) return {};
  const char* raw = env->GetStringUTFChars(value, nullptr);
  std::string result(raw == nullptr ? "" : raw);
  if (raw != nullptr) env->ReleaseStringUTFChars(value, raw);
  return result;
}
}  // namespace

bool InitializeAdminBridge(JavaVM* vm, JNIEnv* env) {
  std::scoped_lock lock(bridge_mutex);
  if (client_class != nullptr && request_method != nullptr) return true;
  auto* local = env->FindClass(
      "org/guitargirlresuscitation/memorial/MemorialAdminClient");
  if (local == nullptr) {
    env->ExceptionClear();
    return false;
  }
  client_class = static_cast<jclass>(env->NewGlobalRef(local));
  env->DeleteLocalRef(local);
  request_method = env->GetStaticMethodID(
      client_class, "request",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;J)Ljava/lang/String;");
  page_method = env->GetStaticMethodID(client_class, "showPage",
      "(Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)V");
  message_method = env->GetStaticMethodID(client_class, "showMessage",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
  dismiss_method = env->GetStaticMethodID(client_class, "dismissPage", "()V");
  identity_method = env->GetStaticMethodID(client_class, "localMemberId", "(J)Ljava/lang/String;");
  list_method = env->GetStaticMethodID(client_class, "showList",
      "(Ljava/lang/String;[Ljava/lang/String;[ZLjava/lang/String;Ljava/lang/String;J)V");
  currency_list_method = env->GetStaticMethodID(client_class, "showCurrencyList", "(Ljava/lang/String;J)V");
  currency_input_method = env->GetStaticMethodID(client_class, "showCurrencyInput", "(Ljava/lang/String;Ljava/lang/String;J)V");
  if (!request_method || !page_method || !message_method || !dismiss_method || !identity_method || !list_method || !currency_list_method || !currency_input_method) {
    env->ExceptionClear();
    env->DeleteGlobalRef(client_class);
    client_class = nullptr;
    return false;
  }
  java_vm = vm;
  return true;
}

// Only marshals UI work. Java posts it to the Activity thread; game callbacks
// return through UnitySendMessage so domain state stays on Unity's thread.
template<class Operation>
void WithPanelEnvironment(Operation operation) {
  std::scoped_lock lock(bridge_mutex);
  if (!java_vm || !client_class) return;
  JNIEnv* env = nullptr;
  bool attached = false;
  if (java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
    if (java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    attached = true;
  }
  if (env && env->PushLocalFrame(16) == JNI_OK) {
    operation(env);
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->PopLocalFrame(nullptr);
  }
  if (attached) java_vm->DetachCurrentThread();
}

void ShowMemorialPage(std::string_view title, const std::array<std::string, 4>& labels,
                      std::string_view close) {
  WithPanelEnvironment([&](JNIEnv* env) {
    auto* strings = env->FindClass("java/lang/String");
    auto* rows = env->NewObjectArray(4, strings, nullptr);
    for (int i = 0; i < 4; ++i) {
      auto* value = env->NewStringUTF(labels[i].c_str());
      env->SetObjectArrayElement(rows, i, value);
      env->DeleteLocalRef(value);
    }
    env->CallStaticVoidMethod(client_class, page_method,
        env->NewStringUTF(std::string(title).c_str()), rows,
        env->NewStringUTF(std::string(close).c_str()));
  });
}

void ShowMemorialMessage(std::string_view title, std::string_view body, std::string_view close) {
  WithPanelEnvironment([&](JNIEnv* env) {
    env->CallStaticVoidMethod(client_class, message_method,
        env->NewStringUTF(std::string(title).c_str()),
        env->NewStringUTF(std::string(body).c_str()),
        env->NewStringUTF(std::string(close).c_str()));
  });
}

void ShowMemorialList(std::string_view title, const std::vector<std::string>& labels,
                      const std::vector<bool>& enabled, std::string_view back,
                      std::string_view close, std::uint32_t generation) {
  if (labels.size() != enabled.size() || labels.size() > 4096) return;
  WithPanelEnvironment([&](JNIEnv* env) {
    auto* strings = env->FindClass("java/lang/String");
    auto* rows = env->NewObjectArray(static_cast<jsize>(labels.size()), strings, nullptr);
    auto* flags = env->NewBooleanArray(static_cast<jsize>(labels.size()));
    for (jsize i = 0; i < static_cast<jsize>(labels.size()); ++i) {
      auto* value = env->NewStringUTF(labels[i].c_str());
      env->SetObjectArrayElement(rows, i, value);
      env->DeleteLocalRef(value);
      const jboolean active = enabled[i];
      env->SetBooleanArrayRegion(flags, i, 1, &active);
    }
    env->CallStaticVoidMethod(client_class, list_method,
        env->NewStringUTF(std::string(title).c_str()), rows, flags,
        env->NewStringUTF(std::string(back).c_str()),
        env->NewStringUTF(std::string(close).c_str()), static_cast<jlong>(generation));
  });
}

void DismissMemorialPage() {
  WithPanelEnvironment([](JNIEnv* env) {
    env->CallStaticVoidMethod(client_class, dismiss_method);
  });
}

void ShowMemorialCurrencyList(std::string_view locale, std::uint32_t generation) {
  WithPanelEnvironment([&](JNIEnv* env) {
    env->CallStaticVoidMethod(client_class, currency_list_method,
        env->NewStringUTF(std::string(locale).c_str()), static_cast<jlong>(generation));
  });
}

void ShowMemorialCurrencyInput(std::string_view currency, std::string_view locale,
                               std::uint32_t generation) {
  WithPanelEnvironment([&](JNIEnv* env) {
    env->CallStaticVoidMethod(client_class, currency_input_method,
        env->NewStringUTF(std::string(currency).c_str()),
        env->NewStringUTF(std::string(locale).c_str()), static_cast<jlong>(generation));
  });
}

std::string LocalMemberId(std::int64_t expected_usn) {
  std::string result;
  WithPanelEnvironment([&](JNIEnv* env) {
    auto* value = static_cast<jstring>(env->CallStaticObjectMethod(
        client_class, identity_method, static_cast<jlong>(expected_usn)));
    if (!env->ExceptionCheck()) result = Utf8(env, value);
  });
  return result;
}

AdminResponse AdminRequest(const std::string_view method,
                           const std::string_view path,
                           const std::string_view body) {
  std::scoped_lock lock(bridge_mutex);
  if (java_vm == nullptr || client_class == nullptr || request_method == nullptr)
    return {};
  JNIEnv* env = nullptr;
  bool attached = false;
  if (java_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) ==
      JNI_EDETACHED) {
    if (java_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return {};
    attached = true;
  }
  const auto to_java = [env](const std::string_view value) {
    return env->NewStringUTF(std::string(value).c_str());
  };
  auto* java_method = to_java(method);
  auto* java_path = to_java(path);
  auto* java_body = to_java(body);
  auto* raw = static_cast<jstring>(env->CallStaticObjectMethod(
      client_class, request_method, java_method, java_path, java_body,
      static_cast<jlong>(NextRequestSequence())));
  env->DeleteLocalRef(java_method);
  env->DeleteLocalRef(java_path);
  env->DeleteLocalRef(java_body);
  AdminResponse result;
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  } else {
    const auto response = Utf8(env, raw);
    const auto newline = response.find('\n');
    if (newline != std::string::npos) {
      const auto first = response.data();
      const auto last = first + newline;
      std::from_chars(first, last, result.status);
      result.body = response.substr(newline + 1);
    }
  }
  if (raw != nullptr) env->DeleteLocalRef(raw);
  if (attached) java_vm->DetachCurrentThread();
  return result;
}

}  // namespace ggfm
