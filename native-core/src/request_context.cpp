#include "ggfm/runtime.hpp"

#include <atomic>
#include <mutex>
#include <ctime>
#if defined(__ANDROID__)
#include <android/log.h>
#endif

namespace ggfm {
namespace {
std::mutex context_mutex;
RequestContext context;
std::atomic<std::uint64_t> request_sequence{0};
}  // namespace

void SetRequestContext(RequestContext next) {
  std::scoped_lock lock(context_mutex);
  context = std::move(next);
  request_sequence.store(0, std::memory_order_release);
}

RequestContext GetRequestContext() {
  std::scoped_lock lock(context_mutex);
  auto current = context;
#if defined(__ANDROID__)
  // A running process may cross a DST transition or a device timezone change.
  // Do not keep sending the offset captured by the startup activity forever.
  const auto now = std::time(nullptr);
  std::tm local{};
  if (localtime_r(&now, &local) != nullptr) {
    const auto minutes = local.tm_gmtoff / 60;
    if (minutes >= -840 && minutes <= 840) {
      current.utc_offset_minutes = static_cast<std::int32_t>(minutes);
      if (context.utc_offset_minutes != current.utc_offset_minutes) {
        __android_log_print(ANDROID_LOG_INFO, "GGFM", "clock: device UTC offset changed %d -> %d minutes",
                            context.utc_offset_minutes, current.utc_offset_minutes);
        context.utc_offset_minutes = current.utc_offset_minutes;
      }
    }
  }
#endif
  return current;
}

std::uint64_t NextRequestSequence() {
  return request_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
}

}  // namespace ggfm
