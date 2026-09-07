# Fractional upgrade price display

The original 8.0.0 Skill and Unit RPC row constructors convert the incoming
double price increment to an integer before converting it to ObscuredFloat.
Memorial increments below one therefore became zero, while the server retained
the fraction and charged the correct higher-level price. Other increments also
lost precision. Both CH1 and CH2 use these shared row types.

After each original RPC constructor, restore only the increment from the DTO as
float32, using the game's own ObscuredFloat conversion. This preserves its value
representation and all other constructor behavior. The SQL reader reads the
increment as a float. The separate binary PlayerPrefs reader and row copy retain
the stored float, including a zero serialized by an older RPC constructor.

The existing client pricing method and UI formatting remain unchanged:
`base + increment * max(0, target_level - first_target_level)`, truncated to an
integer by the UI. First target is 2 for Skill and 1 for Unit. Server charging,
upgrade caps, unlock requirements, currency type and player saves are unchanged.
No player database reset is required. **The server cache generation must advance
as well**: same-day app upgrades can skip RPC reconstruction and reload the old
zero-valued master cache. Server generation 11 requests a fresh master snapshot;
Patch then preserves the fractions before the stock code serializes them again.
The previous statement that a Patch-only restart always reconstructs rows was
incorrect. Do not clear PlayerPrefs or user saves to work around this issue.

ARM64 and ARMv7 constructors, DTO offsets, row offsets and native conversion
entry points are separately fingerprinted. Native tests cover fractional and
integral increments, both ABI layouts, neighboring fields and every target level.
Build/static checks are not a substitute for real-device UI acceptance.

## Device acceptance

On 2026-09-07 the existing main-phone ARM64 installation was updated in place
with Server generation 11 and the fractional-increment Patch, preserving saves.
The user tested the reported upgrade-price issue and confirmed it was working
correctly, then authorized publication. ARMv7 has build/static coverage, not
real-device acceptance for this change.

The following remains the regression checklist; the user confirmation does not
independently establish every chapter and restart case below:

- Upgrade an existing same-day installation without clearing data.
- Confirm the package's actual embedded Server/Patch versions, not just Nightly.
- Observe a new master response and `RPC upgrade increment source=...` logs.
- At a price above 1, compare the skill/furniture label and actual currency delta.
- Repeat after upgrade, reopening the page, and cold restart; cover both chapters.
- Confirm normal same-day restarts do not repeatedly refresh all master tables.
