# Selective adoption of performance PR #2

Source: [PR #2](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch/pull/2),
reviewed at `519489b938a42ac033100cfa5841a5619c26afae`. Credit to the contributor
for locating the repeated fan multiplier work and rewarded-ad retry loop.

## Adopted, with changes

- Stop external rewarded-ad loading at its existing entry on BOTH ABIs. The
  existing local success callbacks remain responsible for rewards. Do not
  change reward quantities, claim persistence or cooldowns.
- Cache the result of the ORIGINAL fan multiplier function. Retain its exact
  floating-point operations on a miss; no replacement formula, rounded value,
  accelerated balance rule, or recomputed prefix product is introduced.
- Cache two areas only, scoped by owner, level and master revision. Unknown
  areas/null owners still use the original function. Fixed-size thread-local
  storage avoids managed GC roots and shared mutable map races.
- Invalidate before/after Fan row insertion/replacement, explicit Clear,
  binary reload and RPC reload, including empty/failed loads. Atomic revision
  invalidation reaches all thread-local entries. The underlying Unity table
  itself still has its original thread-confinement requirements.
- Both profiles require the full hook/dependency set; V7 must not silently
  compile without the actual fixes.

## Deliberately not adopted

No follower-row label hooks, raw row offset, 3-second TTL, 12ms global gate,
row-address map or extra invalidation on player upgrades. The original UI still
refreshes at its original events. Music, achievements, skills, guitars, props,
Fever and character/follower progression therefore keep their native display
semantics. Shared fan calculation was the hotspot inside that label too; first
measure this smaller intervention before considering any UI throttling.

## Static evidence and tests

The private 8.0.0 index confirms the fan product depends on per-area fan level
and Fan master entries. The Fan setter replaces dictionary rows; Initialize
clears the dictionary; RPC/binary loaders also clear it directly. Hence hooking
only Initialize, or only row writes, would miss some empty reloads.

V7 methods were matched by full metadata identity/signature, not translated
addresses. Prologues were read from each original ELF and ARM instruction state
checked. Binary loader return is the two-bool System.Nullable<bool> value type;
the wrapper forwards it unchanged. No client decompilation/resources are stored
in this repository.

Host tests exercise the production replacements with original-function stubs:
10,000 stable reads invoke the original once; area/owner/level changes and all
four mutations invalidate; unknown areas bypass; exact double bits and loader
return values are retained. Profile tests prohibit row throttling and require
all hooks on both ABIs. Android builds use -Werror, sequentially.

Frame rates reported by the PR author on MuMu are NOT our measurements. Host
call-count tests prove eliminated duplicate work, not device frame-rate gains.
Large late-game saves, CH2 and ARMv7 hardware still require runtime performance
acceptance before claiming the original performance report fully resolved.

## Local verification (2026-09-15)

- Both Android native targets compiled successfully with warnings as errors.
- Python suite: 65 tests, 3 skipped on Windows; the production cache wrapper
  test was separately compiled and passed under WSL with warnings as errors.
- Samsung SM-A336E: updated only the native bootstrap split, retaining the
  installed signing identity and save data. Cold startup reached the game;
  logs confirm all five fan hooks and the ad loader hook installed.
- Fan count continued increasing on the home screen. Invoking the offline
  reward ad option reached the existing local flow and logged suppression of
  the external loader. This is a smoke check, not a full reward accounting or
  late-game frame-time benchmark.
- No uninstall, device reboot, save reset, publication or PR merge performed.
