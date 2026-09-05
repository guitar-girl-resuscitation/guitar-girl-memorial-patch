#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <mutex>
#include <string>
#include <string_view>

namespace ggfm {
// Android posts only a selection token; Unity owns all menu and game state.
// A late click from an old page cannot select an item on a newly opened page.
class MenuSelectionQueue {
 public:
  struct Selection { std::uint32_t generation; std::uint32_t index; };
  std::uint32_t Publish() { return generation_.fetch_add(1) + 1; }
  bool Queue(std::uint32_t generation, int index) {
    if (generation == 0 || generation != generation_.load() || index < 0 || index >= 4096) return false;
    std::uint64_t empty = 0;
    const auto token = (std::uint64_t{generation} << 32) | (static_cast<std::uint32_t>(index) + 1);
    return pending_.compare_exchange_strong(empty, token);
  }
  std::optional<Selection> Consume() {
    const auto token = pending_.exchange(0);
    if (!token) return std::nullopt;
    return Selection{static_cast<std::uint32_t>(token >> 32), static_cast<std::uint32_t>(token) - 1};
  }
  bool Current(const Selection& selection) const { return selection.generation == generation_.load(); }
 private:
  std::atomic<std::uint32_t> generation_{0};
  std::atomic<std::uint64_t> pending_{0};
};
// Android queues only bounded values; Unity owns all game state.
class MenuAmountQueue {
 public:
  struct Input { std::uint32_t generation; std::string amount; };
  std::uint32_t Publish() {
    std::scoped_lock lock(mutex_);
    pending_.reset();
    return ++generation_;
  }
  bool Queue(std::uint32_t generation, std::string_view amount) {
    std::scoped_lock lock(mutex_);
    if (generation == 0 || generation != generation_ || pending_ ||
        amount.empty() || amount.size() > 64) return false;
    for (const char c : amount) {
      if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || c == '.' || c == '+')) return false;
    }
    pending_ = Input{generation, std::string(amount)};
    return true;
  }
  std::optional<Input> Consume() {
    std::scoped_lock lock(mutex_);
    auto value = std::move(pending_);
    pending_.reset();
    return value;
  }
  bool Current(const Input& input) {
    std::scoped_lock lock(mutex_);
    return input.generation == generation_;
  }
 private:
  std::mutex mutex_;
  std::uint32_t generation_ = 0;
  std::optional<Input> pending_;
};
}  // namespace ggfm
