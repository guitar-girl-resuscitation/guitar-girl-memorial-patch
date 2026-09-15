# Offline pending-platform-order query

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
