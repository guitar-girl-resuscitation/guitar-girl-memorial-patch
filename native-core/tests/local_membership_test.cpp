// Exercise the actual production replacement with a fake IL2CPP boundary.
// No game process, assets, SDK login, or save file is used by this executable.
#include "../src/runtime_hooks.cpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ggfm {
namespace {
struct Fixture {
  bool missing_identity = false, missing_field = false, commit_exception = false;
  bool logged_in = false, force_logged_out = false, committed = false;
  int succeeded = 0, failed = 0;
  void* fields[3]{};
  bool guest = false;
  std::int64_t usn = 1;
  std::vector<std::unique_ptr<char[]>> strings;
} fixture;
int manager_object, success_delegate, failure_delegate;
int commit_method, check_method, invoke_method, exception_object;

Il2CppString* FakeString(const char* value) {
  const auto count = std::strlen(value);
  auto bytes = std::make_unique<char[]>(sizeof(Il2CppString) + 2 * count);
  auto* result = reinterpret_cast<Il2CppString*>(bytes.get());
  result->length = static_cast<int>(count);
  for (std::size_t i = 0; i < count; ++i) result->chars[i] = value[i];
  fixture.strings.push_back(std::move(bytes));
  return result;
}
void* FakeClass(void* value) { return value; }
const void* FakeMethod(void*, const char* name, int count) {
  if (count == 4) return &commit_method;
  if (!std::strcmp(name, "IsLoggedIn")) return &check_method;
  if (!std::strcmp(name, "Invoke")) return &invoke_method;
  return nullptr;
}
void* FakeField(void*, const char* name) {
  if (fixture.missing_field) return nullptr;
  const char* names[]{"ppMemberId", "ppAccessToken", "ppConflictMemberId", "ppIsGuestLogin"};
  for (int i = 0; i < 4; ++i) if (!std::strcmp(name, names[i]))
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(i + 1));
  return nullptr;
}
void FakeSet(void*, void* field, void* value) {
  const auto index = reinterpret_cast<std::uintptr_t>(field) - 1;
  if (index == 3) fixture.guest = *static_cast<bool*>(value);
  // Verified stock il2cpp_field_set_value -> SetValueRaw(deref=false):
  // reference fields receive the object, primitives receive its storage.
  else fixture.fields[index] = value;
}
void* FakeInvoke(const void* method, void* object, void** args, void** exception) {
  if (method == &commit_method) {
    assert(object == &manager_object);
    assert(*static_cast<bool*>(args[0]) && fixture.guest);
    for (int i = 0; i < 3; ++i) assert(args[i + 1] == fixture.fields[i]);
    assert(Utf8ForLog(static_cast<Il2CppString*>(args[1])) == std::to_string(9000 + fixture.usn));
    assert(Utf8ForLog(static_cast<Il2CppString*>(args[2])) == "ggfm-local-session");
    assert(Utf8ForLog(static_cast<Il2CppString*>(args[3])).empty());
    if (fixture.commit_exception) { *exception = &exception_object; return nullptr; }
    fixture.committed = true;
    fixture.logged_in = !fixture.force_logged_out;
  } else if (method == &check_method) {
    return &fixture.logged_in;
  } else if (method == &invoke_method) {
    if (object == &success_delegate) {
      assert(fixture.committed && fixture.logged_in);
      ++fixture.succeeded;
    } else { assert(object == &failure_delegate); ++fixture.failed; }
  }
  return nullptr;
}
void* FakeUnbox(void* value) { return value; }
}  // namespace
RequestContext GetRequestContext() { return {"http://127.0.0.1:1234", "not-a-publisher-token", fixture.usn, 0}; }
std::string LocalMemberId(std::int64_t expected) {
  assert(expected == fixture.usn);
  return fixture.missing_identity || expected <= 0 ? "" : std::to_string(9000 + expected);
}
}  // namespace ggfm

int main() {
  using namespace ggfm;
  string_new = FakeString;
  object_get_class = FakeClass;
  class_get_method = FakeMethod;
  class_get_field = FakeField;
  field_set_value = FakeSet;
  runtime_invoke = FakeInvoke;
  object_unbox = FakeUnbox;
  for (int scenario = 0; scenario < 7; ++scenario) {
    fixture = Fixture{};
    fixture.missing_identity = scenario == 1;
    fixture.missing_field = scenario == 2;
    fixture.commit_exception = scenario == 3;
    fixture.force_logged_out = scenario == 4;
    fixture.usn = scenario == 5 ? 0 : scenario == 6 ? 2 : 1;
    PmangMembershipLoginHook(&manager_object, &success_delegate, &failure_delegate, nullptr);
    const bool success = scenario == 0 || scenario == 6;
    assert(fixture.succeeded == static_cast<int>(success));
    assert(fixture.failed == static_cast<int>(!success));
    if (scenario == 1 || scenario == 2 || scenario == 5) assert(!fixture.guest);
  }
  std::puts("PASS: actual membership replacement commits identity before success; failures never report success; slot identity is not cached");
}
