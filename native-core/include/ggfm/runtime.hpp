#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace ggfm {

struct RequestContext {
  std::string endpoint;
  std::string capability;
  std::int64_t usn = 0;
  std::int32_t utc_offset_minutes = 0;
  std::int64_t pass_anchor_day = 0;
  int pass_anchor_season = 0;
};

void SetRequestContext(RequestContext context);
void SetPassAnchor(std::int64_t day, int season);
RequestContext GetRequestContext();
std::uint64_t NextRequestSequence();
std::string NamespacePlayerPrefsKey(std::string_view original_key);
std::u16string NamespacePlayerPrefsKey(std::u16string_view original_key);
bool SetActiveUsnOnce(std::int64_t usn);

}  // namespace ggfm
