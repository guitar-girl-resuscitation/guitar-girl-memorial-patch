# Offline pending-platform-order query

## Reproduced boundary

Revision 89 could reach the home scene after removing the task and relaunching,
but retain a translucent full-screen input blocker. Read-only scene inspection
identified `UINetworkLoadingManager` / `Panel - NetworkLoading`, black alpha 0.7,
while `InGameEntryProcess.eStateType` stayed at 9 (`STM_RetryUnfinishPurchase`).
No normal popup was stacked. The last local gameplay RPC, `setAttendance`, had
already returned; the old price Activity had finished and Unity was resumed.

This is a different boundary from the price-catalog Activity fixed in revision
89. Reporting a resumed Unity Activity alone is insufficient startup acceptance.

## Supported client contract

The startup purchase-recovery stage calls the SDK's
`PmangPlus.Unity.PPPlugController.RetryUnfinishedPurchase(PPEventHandler)`.
The SDK's no-pending-orders branch invokes its success listener with:

```json
{"response_code":0,"value":[]}
```

`PPEventHandler.Success` accepts one managed string. The value is an array, not
null or a dictionary. The stock game parses the response and owns completion of
the waiting UI and the entry state. This query is not a new purchase and does not
produce a receipt or grant an item.

## Memorial adaptation

The native shared core completes this platform-only query with the empty result.
The installed package's platform billing is retired; memorial purchases continue
through the independent transactional SQLite server. They are never reported as
recoverable platform orders. The adaptation does not clear saves, alter balances,
skip the entry state machine, force-hide loading widgets, or call remote Billing.

The supported ELF SHA/build-id and target 16-byte prologue are still mandatory.
The handler's `Success` field is resolved through IL2CPP metadata rather than an
unchecked field-offset read. Missing callbacks and invocation exceptions emit an
ERROR; no unrelated success or failure callback is used to conceal the problem.

## Verification

Portable tests validate the empty-array envelope and the fingerprinted SDK
boundary. Native compilation is necessary but not sufficient. Device acceptance
must observe the callback, passage beyond entry state 9, disappearance of network
loading, and working settings controls across task-removal/relaunch with the same
save. Include both first and subsequent launches. Do not claim this tests all
platform SDK callbacks or all gameplay purchases.

### Revision 90 first-launch result (2026-09-05)

- Forty Python tests passed; Android native core rebuilt successfully.
- Installed as an update on the primary phone, without clearing data or
  rebooting the device. XAPK SHA-256:
  `86F9B5A380140D17D80F450618087CB891B54144E157EFB38F9ACC7B22FFF49B`.
- Process 14046 logged `pending platform orders=0; continuation delivered=1`
  at 23:45:32.541. Read-only scene inspection then returned entry state 27
  (`STM_Idle`), network waiting false, no active loading-manager widget, and
  game buttons available. The existing level-15 slot was still selected.
- Private evidence: `build/rev90-cold1.log`, `build/rev90-cold1-q548iquh.png`.
  These files and original-client disassembly remain outside public repos.
- The user subsequently tested recent-task removal and relaunch on the primary
  phone and confirmed normal behavior. This is user-reported repeat-launch
  acceptance in addition to the instrumented first-launch observation above.
