#include "ggfm/gameplay_rules.hpp"
#include "ggfm/client_save.hpp"
#include "ggfm/loopback_boundary.hpp"
#include "ggfm/il2cpp_gc_handle.hpp"
#include <cstdio>

int main() {
  // Reproduce the ARM64 handle shape lost by the old uint32 ABI. Exercise
  // creation, storage, lookup and release through production function types.
  constexpr std::uintptr_t full_handle = 0x0000006f201c0031ULL;
  static_assert(sizeof(void*) == 8);
  const ggfm::WeakHandleNew make_handle = +[](void*, bool) -> std::uintptr_t {
    return full_handle;
  };
  const ggfm::HandleTarget find_handle = +[](std::uintptr_t value) -> void* {
    return value == full_handle ? reinterpret_cast<void*>(value) : nullptr;
  };
  static std::uintptr_t released_handle = 0;
  const ggfm::HandleFree free_handle = +[](std::uintptr_t value) {
    released_handle = value;
  };
  ggfm::Il2CppGcHandle stored_handle = make_handle(nullptr, false);
  if (stored_handle != full_handle || find_handle(stored_handle) == nullptr) return 14;
  free_handle(stored_handle);
  if (released_handle != full_handle) return 15;
  std::puts("PASS: ARM64 GC handle high bits survive create/store/lookup/free");
  sockaddr_in ipv4{};
  ipv4.sin_family = AF_INET;
  for (const char* ip : {"127.0.0.1", "127.0.0.2", "10.0.1.24", "8.8.8.8", "0.0.0.0"}) {
    if (inet_pton(AF_INET, ip, &ipv4.sin_addr) != 1) return 9;
    if (ggfm::IsLocalConnectAddress(reinterpret_cast<sockaddr*>(&ipv4), sizeof(ipv4)) !=
        (std::strcmp(ip, "127.0.0.1") == 0)) return 10;
  }
  sockaddr_in6 ipv6{};
  ipv6.sin6_family = AF_INET6;
  for (const char* ip : {"::1", "::ffff:127.0.0.1", "::ffff:127.0.0.2", "::ffff:10.0.1.24",
                         "::ffff:8.8.8.8", "::127.0.0.1", "2001:db8::1", "::"}) {
    if (inet_pton(AF_INET6, ip, &ipv6.sin6_addr) != 1) return 11;
    const bool expected = std::strcmp(ip, "::1") == 0 || std::strcmp(ip, "::ffff:127.0.0.1") == 0;
    if (ggfm::IsLocalConnectAddress(reinterpret_cast<sockaddr*>(&ipv6), sizeof(ipv6)) != expected) return 12;
  }
  if (ggfm::IsLocalConnectAddress(nullptr, 0) ||
      ggfm::IsLocalConnectAddress(reinterpret_cast<sockaddr*>(&ipv4), sizeof(ipv4) - 1) ||
      ggfm::IsLocalConnectAddress(reinterpret_cast<sockaddr*>(&ipv6), sizeof(ipv6) - 1)) return 13;
  int calls = 0;
  int phase = 0;
  if (ggfm::SaveAfterLocalSuccess([&] { ++calls; return false; }, [&] { ++phase; })) return 5;
  if (calls != 1 || phase != 0) return 6;
  if (!ggfm::SaveAfterLocalSuccess([&] { ++calls; phase = 1; return true; },
                                  [&] { phase = phase == 1 ? 2 : -1; })) return 7;
  if (calls != 2 || phase != 2) return 8;
  for (const bool requested : {false, true}) {
    for (const bool initialized : {false, true}) {
      for (const bool active : {false, true}) {
        if (ggfm::RefreshLoadedContent(requested, initialized, active) !=
            (requested || (initialized && active))) return 4;
      }
    }
  }
  constexpr std::int64_t epoch_ticks = 621355968000000000LL;
  static_assert(ggfm::LocalDayEndTicks(0, 0) == epoch_ticks + 863990000000LL);
  static_assert(ggfm::LocalDayEndTicks(-1, 0) == epoch_ticks - 10000000LL);
  static_assert(ggfm::LocalDayEndTicks(86399, 0) == ggfm::LocalDayEndTicks(0, 0));
  static_assert(ggfm::LocalDayEndTicks(86400, 0) == epoch_ticks + 1727990000000LL);
  static_assert(ggfm::LocalDayEndTicks(23 * 3600, 120) ==
                ggfm::LocalDayEndTicks(86400, 0));
  static_assert(ggfm::LocalDayEndTicks(0, -60) == epoch_ticks - 10000000LL);
  // Reproducer: 18:00 UTC is 13:00 UTC-5, but the game's clock is already
  // 03:00 the next day. An unconverted local deadline incorrectly hides Pass.
  static_assert(ggfm::LocalDayEndTicks(18 * 3600, -300) <
                epoch_ticks + 27LL * 3600 * 10000000LL);
  static_assert(ggfm::PassDayEndTicks(18 * 3600, -300) >
                epoch_ticks + 27LL * 3600 * 10000000LL);
  for (const int offset : {-720, -360, -300, -210, 0, 330, 540, 600, 840}) {
    // Include pre-epoch, month/year boundaries, and every hour on either side
    // of midnight. At every instant the selected day's window includes NOW.
    for (const std::int64_t anchor : {-86400LL, 0LL, 1798761600LL}) {
      for (int hour = -24; hour <= 48; ++hour) {
        const auto now = anchor + hour * 3600 + 17;
        const auto clock = epoch_ticks + (now + 9 * 3600) * 10000000LL;
        const auto start = ggfm::PassDayStartTicks(now, offset);
        const auto end = ggfm::PassDayEndTicks(now, offset);
        if (clock < start || clock > end || end - start != 86399LL * 10000000LL) return 16;
        const auto next = (end - epoch_ticks) / 10000000LL - 9 * 3600 + 1;
        if (ggfm::PassDayStartTicks(next, offset) != end + 10000000LL) return 17;
      }
    }
  }
  ggfm::QuestClaimProjection claims;
  claims.Update(1, {1, 0, 1});
  if (claims.State(1, 1, 0) != 2 || claims.State(1, 2, 1) != 1 ||
      claims.State(1, 3, 0) != 2 || claims.State(2, 1, 0) != 0 ||
      claims.State(1, 0, 7) != 7 || claims.State(1, 4, 7) != 7) return 1;
  claims.Update(1, {0, 1, 0});
  if (claims.State(1, 1, 0) != 0 || claims.State(1, 2, 0) != 2 ||
      claims.State(1, 3, 0) != 0) return 2;
  claims.Update(2, {0, 0, 0});
  if (claims.State(2, 2, 0) != 0 || claims.State(1, 2, 0) != 0) return 3;
  std::puts("PASS: loopback boundary, save ordering, late bindings, daily deadlines, claims and USN isolation");
  return 0;
}
