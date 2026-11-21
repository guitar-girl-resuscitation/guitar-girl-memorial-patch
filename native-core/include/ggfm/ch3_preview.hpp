#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace ggfm::ch3 {
// Offsets from the two v8 StageReady, selected-slot and profile layouts.
struct Layout {
  unsigned stage, slots, guest, master, user, hints;
};
constexpr Layout layout64{0x58, 0x68, 0x18, 0x20, 0x48, 0x48};
constexpr Layout layout32{0x30, 0x38, 0x0c, 0x10, 0x24, 0x30};

template<class T> T read(const void* object, unsigned offset) {
  T value{};
  if (object) std::memcpy(&value, static_cast<const char*>(object) + offset, sizeof(value));
  return value;
}

struct Preview {
  int stage{}, hints{}, count{}, next{};
  struct Guest { int level{}; bool hinted{}; };
  std::array<Guest, 3> guests{};
};

inline Preview snapshot(void* panel) {
  constexpr unsigned ptr = sizeof(void*);
  constexpr Layout offsets = ptr == 8 ? layout64 : layout32;
  Preview result;
  auto stage = read<void*>(panel, offsets.stage);
  result.stage = read<int>(stage, 2 * ptr);
  auto hint = read<void*>(stage, offsets.hints);
  const int length = read<int>(hint, 2 * ptr);
  if (length <= 0 || length > 256) return result;
  std::array<int, 32> ids{};
  int id = 0;
  for (int i = 0; i <= length; ++i) {
    const auto c = i == length ? u',' : read<char16_t>(hint, 2 * ptr + 4 + 2 * i);
    if (c >= u'0' && c <= u'9') {
      if (id > 1000000) return {};
      id = id * 10 + c - u'0';
    } else if (id) {
      if (result.hints == static_cast<int>(ids.size())) return {};
      ids[result.hints++] = id;
      id = 0;
    }
  }
  auto slots = read<void*>(panel, offsets.slots);
  auto items = read<void*>(slots, 2 * ptr);
  const int size = read<int>(slots, 3 * ptr);
  if (!items || size < 0 || size > 5 ||
      static_cast<unsigned>(size) > read<std::uintptr_t>(items, 3 * ptr)) return {};
  for (int i = 0; i < size; ++i) {
    auto slot = read<void*>(items, (4 + i) * ptr);
    auto guest = read<void*>(slot, offsets.guest);
    auto user = read<void*>(guest, offsets.user);
    auto master = read<void*>(guest, offsets.master);
    const int profile = read<int>(master, 2 * ptr);
    if (!user || !master || profile == 100000) continue;
    if (result.count == 3) return {};
    auto& entry = result.guests[result.count++];
    entry.level = read<int>(user, 2 * ptr + 4);
    for (int j = 0; j < result.hints; ++j) entry.hinted |= profile == ids[j];
  }
  return result;
}
}  // namespace ggfm::ch3
