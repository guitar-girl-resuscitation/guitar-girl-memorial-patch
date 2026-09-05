#pragma once

namespace ggfm {
// Catalog query only: the standalone build has no platform-billing orders.
// Keep value an array (not null, a dictionary or a purchase-success receipt).
inline constexpr char kEmptyPendingPlatformOrders[] =
    R"({"response_code":0,"value":[]})";
}  // namespace ggfm
