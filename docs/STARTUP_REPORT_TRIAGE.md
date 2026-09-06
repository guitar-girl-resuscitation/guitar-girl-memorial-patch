# Community startup reports (2026-09-06)

Do not conflate these failure stages:

- `native startup result -6` with a catalog open error: native bootstrap and
  Rust were already running; investigate the read-only master catalog separately
  from the player database. Clicking General Store before a crash is chronology,
  not proof of the failing catalog operation's cause.
- `bootstrap: UnsatisfiedLinkError` before banner/startServer: Java failed to
  load/link or resolve a native entry point. The original UI discarded the
  exception message, so that screenshot cannot identify a library, symbol or ABI.

The banner catch now displays the message/cause chain, SDK, supported ABIs,
native library directory and installed split paths. Rust catalog failures retain
the debug error (including SQLite detail) and file stat evidence. These are
diagnostic improvements, not a reproduced fix for either community crash.

Request the Android version/device model, installed build/version and installer,
full exception detail or the crash-time logcat; distinguish the original crash
from a subsequent startup failure. Never recommend clearing distributed saves
to mask either symptom.

Fresh server slots seed currencies at zero. A regression switches from an old
slot holding 12 Likes/500 Candy into a new slot and checks all currencies zero
and no CH3 completion. This does not prove why a user sees 12 Likes after the
client tutorial starts. The early CH3 extra-node issue is completion-history
projection, documented in the server repository; it is not evidence of a bundled
player save. Filename inspection of the three private 800014 output splits found
no ggfm.sqlite, PlayerPrefs/shared_prefs or save_slots payload candidates; that
limited inspection is not a claim about every older deployment's output.
