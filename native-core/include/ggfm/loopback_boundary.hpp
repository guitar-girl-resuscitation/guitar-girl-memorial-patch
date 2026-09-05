#pragma once

#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>

namespace ggfm {

// Android's Java networking uses dual-stack sockets even for numeric IPv4
// URLs. ::ffff:127.0.0.1 is exactly the same allowed endpoint as 127.0.0.1.
// Do not admit mapped public/LAN addresses or widen this to the whole 127/8.
inline bool IsLocalConnectAddress(const sockaddr* address, socklen_t length) {
  if (!address || length < sizeof(sa_family_t)) return false;
  if (address->sa_family == AF_UNIX) return true;
  if (address->sa_family == AF_INET) {
    if (length < sizeof(sockaddr_in)) return false;
    return ntohl(reinterpret_cast<const sockaddr_in*>(address)->sin_addr.s_addr)
        == INADDR_LOOPBACK;
  }
  if (address->sa_family == AF_INET6) {
    if (length < sizeof(sockaddr_in6)) return false;
    const auto& ip = reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
    if (IN6_IS_ADDR_LOOPBACK(&ip)) return true;
    if (!IN6_IS_ADDR_V4MAPPED(&ip)) return false;
    in_addr_t mapped;
    std::memcpy(&mapped, &ip.s6_addr[12], sizeof(mapped));
    return ntohl(mapped) == INADDR_LOOPBACK;
  }
  return false;
}

}  // namespace ggfm
