#include "ggfm/upgrade_increment.hpp"
#include <array>
#include <cassert>
#include <limits>

ggfm::UpgradeEncodedFloat Encode(float value, const void*) {
  ggfm::UpgradeEncodedFloat result{{0, 11, 22, 33, 44}};
  std::memcpy(&result.words[0], &value, sizeof(value));
  return result;
}

int main() {
  for (auto source_offset : {0x88u, 0x90u, 0xF8u}) {
    for (auto target_offset : {0xB0u, 0x11Cu}) {
      std::array<char, 512> dto{}, row{};
      row.fill(42);
      for (double increment : {0.0, 0.5, 9.0/19, 14.0/18, 19.0/18, 10.0}) {
        std::memcpy(dto.data() + source_offset, &increment, sizeof(increment));
        assert(ggfm::RestoreUpgradeIncrement(row.data(), dto.data(), source_offset, target_offset, Encode));
        ggfm::UpgradeEncodedFloat encoded{};
        std::memcpy(&encoded, row.data() + target_offset, sizeof(encoded));
        float restored = 0;
        std::memcpy(&restored, &encoded.words[0], sizeof(restored));
        assert(restored == static_cast<float>(increment));
        assert(encoded.words[4] == 44);
        assert(row[target_offset-1] == 42 && row[target_offset+20] == 42);
        // All target levels, including final paid upgrade, match server float32.
        for (int first : {1, 2}) for (int level = first; level <= 20; ++level) {
          assert(static_cast<int>(1.0f + restored * (level-first)) ==
                 static_cast<int>(1.0f + static_cast<float>(increment) * (level-first)));
        }
      }
      auto previous = row;
      double invalid = std::numeric_limits<double>::infinity();
      std::memcpy(dto.data() + source_offset, &invalid, sizeof(invalid));
      assert(!ggfm::RestoreUpgradeIncrement(row.data(), dto.data(), source_offset, target_offset, Encode));
      assert(row == previous);
      assert(!ggfm::RestoreUpgradeIncrement(nullptr, dto.data(), source_offset, target_offset, Encode));
    }
  }
}
