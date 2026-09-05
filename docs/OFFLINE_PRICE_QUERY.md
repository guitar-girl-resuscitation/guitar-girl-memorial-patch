# Offline price-query continuation

The second cold launch of revision 88 was blocked by the retired PMang price
lookup Activity, not a dead Rust server. Device activity state showed
`PPPurchaseGoogleLocalPrice_port` resumed with the Unity host paused behind it.
The price lookup had no gameplay UI and could wait indefinitely for Billing.

## Supported 8.0.0 contract

- PMang explicitly launches either `PPPurchaseGoogleLocalPrice_port` or
  `PPPurchaseGoogleLocalPrice` to query localized real-money prices. This is
  distinct from committing a purchase or granting its rewards.
- The stock `JSONManager.invokeOnLocalPrice(ArrayList, String)` delivers
  `PPDelegate.onPriceLocalization` with `response_code=0` and a value containing
  `price_currency_code`, `price` and `price_old` dictionaries.
- An empty list produces non-null empty dictionaries and bypasses the SDK's
  remote price-cache update branch. The game's price callback accepts an empty
  dictionary and leaves existing display values unchanged.
- Memorial purchases already use the independent local-server transaction
  path. This shim creates no receipts, products, inventory or account data.

## Implementation

The manifest transformer requires exactly one of each stock price Activity.
It replaces their component declarations with non-exported activity aliases
pointing to our own `LocalPriceActivity`, declared before both aliases. The
original explicit Intent names still resolve, but their Billing `onCreate`
methods do not run. Our NoDisplay Activity delivers the stock callback with an
empty catalog and finishes in `onCreate`. Failures log the callback contract
error; they are not represented as successful purchases.

No original DEX method body, game prefab or artwork is stored in this change.
The patch pipeline still requires the complete source/split hashes before any
manifest transformation. Missing/duplicate components fail closed.

## Acceptance boundary

Manifest tests verify both aliases, target ordering, non-exported status and
missing/duplicate input rejection. Bootstrap DEX compilation checks Android
types, while the actual original bridge signature was verified against the
supported user-supplied package. Device acceptance requires two cold launches
with the same save, a logged offline callback and return to the Unity game.

## Revision 89 device result

- 38 Python tests and the standalone Java terminal/localization tests passed.
- Revision 89 was installed as an update over 88, with no data clearing.
  XAPK SHA-256: `C52D57881660F9FCC58FC44B6554C5A90D38AED90A863C37D45F1A752E1A393A`.
- First cold launch: process 24776, offline price callback at 23:13:50; normal
  home screen, character level 15 and 86 fans. Second cold launch: process
  25450, callback at 23:15:36; same home/save, no blocked price Activity.
  Both were observed on the primary phone on 2026-09-05, without phone reboot.
- Private diagnostic evidence is outside the repository: `build/rev89-cold1.log`,
  `build/rev89-cold2.log` and corresponding loaded-screen screenshots. Neither
  gameplay screenshots nor device saves belong in published release artifacts.
- Quick start remains off by default; all twelve labels omit the parenthetical
  explanation. Existing explicit user preference remains installation-scoped.
- This verifies the cold-start price-query continuation, not a new exhaustive
  acceptance of every shop action. Publication remains paused for user retest.
