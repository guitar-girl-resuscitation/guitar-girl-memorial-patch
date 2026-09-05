#include "ggfm/runtime.hpp"

#include <atomic>
#include <mutex>

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
  return context;
}

std::uint64_t NextRequestSequence() {
  return request_sequence.fetch_add(1, std::memory_order_acq_rel) + 1;
}

}  // namespace ggfm
