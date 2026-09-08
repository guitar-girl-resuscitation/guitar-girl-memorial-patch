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

Tests reproduce the previous early expiry and check inclusion, day length and
rollover for nine offsets including the reported UTC-6, negative epochs and
year-boundary dates. Native host tests and both Android builds passed.
Samsung SM-A336E was tested with a fixed UTC-6 timezone: the entry was visible,
season 8 opened with both reward tracks, and its countdown was 3h10m at local
20:50. Cold reopening retained the entry. This is Samsung ARM64 acceptance,
not a test of the reporting user's device or ARMv7 hardware.

Native request context now reads Android's current offset per request instead
of retaining the startup offset across DST/timezone changes. A live Samsung
change produced `clock: device UTC offset changed 600 -> -360 minutes` without
restarting the process or resetting request sequence/identity. The stock pass
UI caches its loaded window: after changing timezone during play, cold reopen
is still needed to reload the visible season/window. Do not claim seamless
live UI refresh from this request-header fix.

The test device timezone and automatic timezone setting were restored. No
data/cache clearing or save migration is required.
