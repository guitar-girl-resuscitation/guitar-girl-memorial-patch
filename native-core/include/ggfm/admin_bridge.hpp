#pragma once

#include <jni.h>

#include <cstdint>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace ggfm {

struct AdminResponse {
  int status = 599;
  std::string body;

  bool ok() const { return status >= 200 && status < 300; }
};

bool InitializeAdminBridge(JavaVM* vm, JNIEnv* env);
void ShowMemorialPage(std::string_view title, const std::array<std::string, 4>& labels,
                      std::string_view close);
void ShowMemorialMessage(std::string_view title, std::string_view body, std::string_view close);
void ShowMemorialList(std::string_view title, const std::vector<std::string>& labels,
                      const std::vector<bool>& enabled, std::string_view back,
                      std::string_view close, std::uint32_t generation);
void DismissMemorialPage();
void ShowMemorialCurrencyInput(std::string_view currency, std::string_view locale,
                               std::uint32_t generation);
void ShowMemorialCurrencyList(std::string_view locale, std::uint32_t generation);
std::string LocalMemberId(std::int64_t expected_usn);
AdminResponse AdminRequest(std::string_view method, std::string_view path,
                           std::string_view body = {});

}  // namespace ggfm
