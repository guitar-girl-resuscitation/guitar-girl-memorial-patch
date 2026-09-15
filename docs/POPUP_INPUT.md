# Popup input completion

The original opening and closing animations are preserved. EndScale first clears
the animator busy flag and settles alpha, then the stored open callback runs once.
Clicks during the opening animation can still be ignored; this is not a complete
fix for early confirmation.

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
