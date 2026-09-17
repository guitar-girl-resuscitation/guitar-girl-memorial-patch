#pragma once

#include <string>
#include <string_view>

namespace ggfm {

// Restrict normalization to the processed field of the label currently wrapping.
// Nested label processing temporarily replaces, then restores, this context.
struct PlainLabelContext {
  void* owner = nullptr;
  void* output = nullptr;
  bool encoding = true;
};

class PlainLabelScope {
 public:
  PlainLabelScope(PlainLabelContext& current, PlainLabelContext next)
      : current_(current), previous_(current) { current_ = next; }
  ~PlainLabelScope() { current_ = previous_; }
  PlainLabelScope(const PlainLabelScope&) = delete;
  PlainLabelScope& operator=(const PlainLabelScope&) = delete;
 private:
  PlainLabelContext& current_;
  PlainLabelContext previous_;
};

// Empty means no replacement. Never strip rich-text alpha/color resets: they
// have real semantics. Preserve all line breaks and all caller-supplied text.
inline std::u16string PlainEllipsis(const PlainLabelContext& context,
                                  const void* output, bool ellipsis,
                                  std::u16string_view input,
                                  std::u16string_view wrapped) {
  constexpr std::u16string_view suffix = u"[-][ff]...";
  if (!context.owner || context.output != output || context.encoding || !ellipsis ||
      !wrapped.ends_with(suffix) || input == wrapped ||
      input.find(suffix) != std::u16string_view::npos) {
    return {};
  }
  std::u16string result(wrapped.substr(0, wrapped.size() - suffix.size()));
  result += u"...";
  return result;
}

}  // namespace ggfm
