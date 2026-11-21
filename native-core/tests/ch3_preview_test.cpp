#include "ggfm/ch3_preview.hpp"

namespace {
struct alignas(8) Object {
  char data[256]{};
  template<class T> void put(unsigned offset, T value) {
    std::memcpy(data + offset, &value, sizeof(value));
  }
};
}

// Exercise the recovered v8 layouts with an empty music slot, Lily, a
// non-hinted guest and Alex in different positions at the same level.
extern "C" int check_ch3_preview() {
  constexpr unsigned p = sizeof(void*);
  constexpr bool wide = p == 8;
  Object panel, stage, hints, list, array, slots[5], profiles[4], masters[4], users[4];
  panel.put(wide ? 0x58 : 0x30, &stage);
  panel.put(wide ? 0x68 : 0x38, &list);
  stage.put(p * 2, 2018);
  stage.put(wide ? 0x48 : 0x30, &hints);
  constexpr char16_t text[] = u"205, 15";
  hints.put(2*p, 7);
  std::memcpy(hints.data + 2*p + 4, text, sizeof(text));
  list.put(2*p, &array);
  list.put(3*p, 5);
  array.put(3*p, std::uintptr_t{5});
  constexpr int ids[]{100000, 1, 205, 15};
  for (int i = 0; i < 5; ++i) {
    array.put((4+i)*p, &slots[i]);
    if (i == 0) continue;
    const int j = i - 1;
    slots[i].put(3*p, &profiles[j]);
    profiles[j].put(4*p, &masters[j]);
    profiles[j].put(9*p, &users[j]);
    masters[j].put(2*p, ids[j]);
    users[j].put(2*p + 4, 10);
  }
  auto preview = ggfm::ch3::snapshot(&panel);
  if (preview.stage != 2018 || preview.hints != 2 || preview.count != 3) return 1;
  if (preview.guests[0].hinted || !preview.guests[1].hinted || !preview.guests[2].hinted) return 2;
  for (const auto& guest : preview.guests) if (guest.level != 10) return 3;
  profiles[2].put(9*p, static_cast<void*>(nullptr));
  preview = ggfm::ch3::snapshot(&panel);
  if (preview.count != 2 || preview.guests[0].hinted || !preview.guests[1].hinted) return 4;
  return 0;
}
int main() { return check_ch3_preview(); }
