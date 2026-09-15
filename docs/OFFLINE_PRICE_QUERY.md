# Offline price-query continuation

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
