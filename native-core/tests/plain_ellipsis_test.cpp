#include "ggfm/plain_ellipsis.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

using namespace ggfm;
struct Il2CppString { std::int32_t length; char16_t chars[256]; };
struct Label { bool encoding; Il2CppString* processed; };
constexpr std::size_t RuntimeField(std::string_view key) {
  return key == "label.encoding" ? offsetof(Label, encoding) : offsetof(Label, processed);
}
Il2CppString source{}, wrapped{}, allocated{};
int allocations = 0, writes = 0;
void* written_owner = nullptr;
Il2CppString* NewString(const char16_t* data, std::int32_t length) {
  ++allocations;
  assert(length < 256);
  allocated.length = length;
  std::copy_n(data, length, allocated.chars);
  return &allocated;
}
void Barrier(void* owner, void** slot, void* value) {
  ++writes;
  written_owner = owner;
  *slot = value;
}
auto string_new_utf16 = NewString;
auto popup_clear = Barrier;
#include "runtime_wrappers.inc"

void Assign(Il2CppString& s, std::u16string_view value) {
  assert(value.size() < 256);
  s.length = static_cast<std::int32_t>(value.size());
  std::copy(value.begin(), value.end(), s.chars);
}
std::u16string_view View(const Il2CppString* s) {
  return {s->chars, static_cast<std::size_t>(s->length)};
}
bool fit = false;
bool MockWrap(Il2CppString* input, Il2CppString** out, bool keep,
              bool colors, bool ellipsis, const void* method) {
  assert(input == &source && !keep && !colors && ellipsis && !method);
  *out = &wrapped;
  return fit;
}
std::u16string measured;
void MockProcess(void* owner, bool legacy, bool full, const void* method) {
  assert(!legacy && full && !method);
  auto* label = static_cast<Label*>(owner);
  assert(WrapLabelText(&source, &label->processed, false, false, true, nullptr) == fit);
  // The original caller measures immediately after WrapText, not after ProcessText.
  measured = View(label->processed);
}
int main() {
  int owner, slot, other;
  PlainLabelContext context{&owner, &slot, false};
  assert(PlainEllipsis(context, &slot, true, u"abcdef", u"abc[-][ff]...") == u"abc...");
  assert(PlainEllipsis(context, &slot, true, u"中文\n消息正文", u"中文\n消[-][ff]...") == u"中文\n消...");
  assert(PlainEllipsis(context, &slot, true, u"abcdef", u"abc...").empty());
  assert(PlainEllipsis(context, &other, true, u"abcdef", u"abc[-][ff]...").empty());
  assert(PlainEllipsis(context, &slot, false, u"abcdef", u"abc[-][ff]...").empty());
  assert(PlainEllipsis(context, &slot, true, u"abc[-][ff]...", u"abc[-][ff]...").empty());
  assert(PlainEllipsis(context, &slot, true, u"literal end [-][ff]...", u"lit[-][ff]...").empty());
  assert(PlainEllipsis(context, &slot, true, u"literal [-][ff]... continues", u"literal [-][ff]...").empty());
  assert(PlainEllipsis(context, &slot, true, u"abcdef", u"abc[-][ff]...\n").empty());
  {
    PlainLabelScope nested(context, {&other, &other, true});
    assert(PlainEllipsis(context, &other, true, u"abcdef", u"abc[-][ff]...").empty());
  }
  assert(context.owner == &owner && context.output == &slot && !context.encoding);

  original_process_label = MockProcess;
  original_wrap_label = MockWrap;
  Assign(source, u"a long message");
  Assign(wrapped, u"a long[-][ff]...");
  Label label{false, nullptr};
  for (const bool result : {false, true}) {
    fit = result;
    ProcessLabelText(&label, false, true, nullptr);
    assert(measured == u"a long...");
    assert(written_owner == &label);
    assert(!plain_label_context.owner);
  }
  assert(allocations == 2 && writes == 2);
  label.encoding = true;
  ProcessLabelText(&label, false, true, nullptr);
  assert(measured == u"a long[-][ff]..." && allocations == 2);
  Il2CppString* unrelated = nullptr;
  WrapLabelText(&source, &unrelated, false, false, true, nullptr);
  assert(unrelated == &wrapped && allocations == 2);
  label.encoding = false;
  Assign(wrapped, u"fits");
  ProcessLabelText(&label, false, true, nullptr);
  assert(measured == u"fits" && allocations == 2);
}
