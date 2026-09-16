#include "ggfm/large_number_display.hpp"
#include <cassert>
#include <vector>

int main() {
  std::vector<std::uint32_t> limbs(218, 0);
  limbs.back() = 1;
  assert(ggfm::LargeNumberPreview(nullptr, 0, false, 3).empty());
  assert(ggfm::LargeNumberPreview(limbs.data(), limbs.size(), false, 3).empty());
  // 2^7008 is above the final stock ZZ suffix; formatting must be bounded.
  limbs.resize(220, 0);
  limbs.back() = 1;
  auto text = ggfm::LargeNumberPreview(limbs.data(), limbs.size(), false, 3);
  assert(text.find("e2109") != std::string::npos && text.size() < 20);
  assert(ggfm::LargeNumberPreview(limbs.data(), limbs.size(), true, 3) == "-" + text);
  // Even an enormous limb count only reads the two leading words.
  limbs.resize(10000, 0);
  limbs.back() = 1;
  assert(ggfm::LargeNumberPreview(limbs.data(), limbs.size(), false, 999).size() < 30);
}
