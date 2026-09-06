# Fractional upgrade price display

The original 8.0.0 Skill and Unit RPC row constructors convert the incoming
double price increment to an integer before converting it to ObscuredFloat.
Memorial increments below one therefore became zero, while the server retained
the fraction and charged the correct higher-level price. Other increments also
lost precision. Both CH1 and CH2 use these shared row types.

After each original RPC constructor, restore only the increment from the DTO as
float32, using the game's own ObscuredFloat conversion. This preserves its value
representation and all other constructor behavior. The SQL reader already reads
the increment as a float. Row copy/serialization retains that float.

The existing client pricing method and UI formatting remain unchanged:
`base + increment * max(0, target_level - first_target_level)`, truncated to an
integer by the UI. First target is 2 for Skill and 1 for Unit. Server charging,
upgrade caps, unlock requirements, currency type and player saves are unchanged.
No database reset is required; restart with the updated Patch to reconstruct rows.

ARM64 and ARMv7 constructors, DTO offsets, row offsets and native conversion
entry points are separately fingerprinted. Native tests cover fractional and
integral increments, both ABI layouts, neighboring fields and every target level.
Build/static checks are not a substitute for real-device UI acceptance.
