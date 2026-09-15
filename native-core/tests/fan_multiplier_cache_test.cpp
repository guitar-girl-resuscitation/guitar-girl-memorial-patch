// Exercise the actual replacement/trampoline code with authored fake tables.
#include "../src/fan_multiplier_cache.cpp"
#include <bit>
#include <cassert>
#include <cstdio>
#include <limits>

namespace {
int level = 40;
int calculations = 0;
int forwarded = 0;
double result = 2.5;
int GetLevel(void*, std::int32_t, const void*) { return level; }
double Calculate(void*, std::int32_t, const void*) { ++calculations; return result; }
void Clear(void*, const void*) { ++forwarded; }
void SetRow(void*, std::int32_t id, void* row, const void*) {
  assert(id == 9 && row != nullptr); ++forwarded;
}
bool LoadRpc(void*, void*, const void*) { ++forwarded; return false; }
ggfm::NullableBool LoadBytes(void*, void*, const void*) { ++forwarded; return {true, false}; }
template<class T> T Bind(std::string_view name, T original) {
  const auto binding = ggfm::ResolveFanMultiplierCacheHook(name);
  assert(binding.replacement && binding.original);
  *binding.original = reinterpret_cast<void*>(original);
  return reinterpret_cast<T>(binding.replacement);
}
}

int main() {
  assert(ggfm::InitializeFanMultiplierCache(reinterpret_cast<std::uintptr_t>(GetLevel)));
  const auto calculate = Bind("gameplay.fan.multiplier", Calculate);
  const auto clear = Bind("gameplay.fan.clear", Clear);
  const auto set_row = Bind("gameplay.fan.setRow", SetRow);
  const auto load_rpc = Bind("gameplay.fan.loadRpc", LoadRpc);
  const auto load_bytes = Bind("gameplay.fan.loadBytes", LoadBytes);
  int owner_a = 0, owner_b = 0;
  for (int i = 0; i < 10000; ++i) assert(calculate(&owner_a, 1, nullptr) == 2.5);
  assert(calculations == 1);
  calculate(&owner_a, 2, nullptr); assert(calculations == 2);
  calculate(&owner_b, 1, nullptr); assert(calculations == 3);
  --level; calculate(&owner_b, 1, nullptr); assert(calculations == 4);
  ++level; calculate(&owner_b, 1, nullptr); assert(calculations == 5);
  result = 7.0;
  clear(&owner_b, nullptr);
  assert(calculate(&owner_b, 1, nullptr) == 7.0 && calculations == 6);
  set_row(&owner_b, 9, &owner_a, nullptr);
  calculate(&owner_b, 1, nullptr); assert(calculations == 7);
  assert(!load_rpc(&owner_b, nullptr, nullptr));
  calculate(&owner_b, 1, nullptr); assert(calculations == 8);
  const auto nullable = load_bytes(&owner_b, nullptr, nullptr);
  assert(nullable.has_value && !nullable.value);
  calculate(&owner_b, 1, nullptr); assert(calculations == 9 && forwarded == 4);
  calculate(&owner_b, 99, nullptr); calculate(&owner_b, 99, nullptr);
  assert(calculations == 11); // unknown area is passed through, never cached
  calculate(nullptr, 1, nullptr); assert(calculations == 12);
  for (double exact : {-0.0, 0.1, std::numeric_limits<double>::infinity()}) {
    result = exact; clear(&owner_b, nullptr);
    for (int i = 0; i < 2; ++i)
      assert(std::bit_cast<std::uint64_t>(calculate(&owner_b, 1, nullptr)) ==
             std::bit_cast<std::uint64_t>(exact));
  }
  std::puts("PASS: original result preserved; 10000 reads / 1 calculation; owner, area, level and all master mutations invalidate");
}
