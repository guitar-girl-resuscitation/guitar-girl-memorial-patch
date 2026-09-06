# Popup input completion

PR #1 identified an input-dead period after a scale popup appears settled.
Device testing found that moving the callback to EndScale alone still leaves
about 0.55 seconds of opening tween during which early confirmation is ignored.
An instant-open experiment was rejected for its visual feel and reverted.
The original opening and closing animations are preserved. EndScale first clears
the animator busy flag and settles alpha, then the stored open callback runs once.
Clicks during the opening animation can still be ignored; this is not yet a
complete fix for the reported early-confirmation behavior. Do not publish this
as a fully validated first-click fix.

Release decision (2026-09-07): the user tested the animation-preserving build on
their ARM64 phone, reported improved overall feel and approved publishing this
partial improvement. The instant-open experiment is NOT included. ARMv7 builds
and mapping checks pass; this popup change has not had ARMv7 device acceptance.

The wait coroutine's final callback-only state is consumed without dispatch.
Leaving it active would let a delayed coroutine invoke a callback belonging to
a subsequent opening of the same cached popup. Automatic/fallback animation
completion retains the EndScale path. The callback is strongly GC-rooted, cleared with a write barrier
before managed invocation, and released afterwards, including invocation failure.

Both exact 8.0.0 ARM64 and ARMv7 manifests fingerprint EndScale and WaitOpen
MoveNext and specify callback, close flag and coroutine state offsets. There is
no server reward or purchase change. Position-only popup animations are outside
this change; they have a separate implementation and no equivalent Open(Action).

The Shop, ShopDetail, InfoFanCostume and UnlockFanCostume confirmation overrides
also check a global one-second click cooldown. The preceding row's click can
therefore suppress the first confirmation even after the animation is complete.
For these four verified debounce-only overrides, confirmation instead claims
the loaded popup (state 2 -> closing 3) and calls stock Base.OnUIEventOK. Repeated
confirmation during closing is ignored. Other global debounce users are unchanged;
no purchase or reward is dispatched directly by the hook.

Acceptance: test a quick single confirm after the card settles, quick background
cancel, follower affection reward confirmation, repeated close/reopen, and
multiple successive rewards. Verify one business RPC/reward per intentional
confirmation and no callback from an earlier popup changes the current popup.
Native tests cover at-most-once dispatch, reentrant opening, stale waits, close
guard and GC-root release. They are not a substitute for device acceptance.
