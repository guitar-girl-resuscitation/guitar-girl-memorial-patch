# Daily Star Pass visibility across timezones

The stock 8.0.0 RPC clock converts Unix seconds to DateTime using an epoch
of 1970-01-01 09:00. SubscribeList compares its start/end ticks with that clock.
The previous patch wrote device-local wall-time midnight into the end field,
mixing two clock representations. In UTC-5, for example, the button could be
hidden at 13:00 local because the game's comparison clock was already on the
following day. Switching season or language cannot correct that comparison.

Compute midnight using the device offset, then convert that instant to the
stock UTC+9 tick representation. Replace both adjacent start/end DateTime fields
(ARM64 0x30/0x38; ARMv7 0x28/0x30) in the existing fingerprinted SetWindow hook.
Do not change the active flag, entitlement, selected season, saved progress,
or the global game clock. Rotation remains at DEVICE midnight, not Korea midnight.

Regression tests should check window inclusion, day length and rollover across
UTC offsets, negative epochs and year boundaries.

Native request context reads Android's current offset per request instead of
retaining the startup offset across DST/timezone changes. The stock pass UI
caches its loaded window: after changing timezone during play, cold reopening
is still needed to reload the visible season/window. Request-header updates do
not imply seamless live UI refresh. No data clearing or save migration is needed.
