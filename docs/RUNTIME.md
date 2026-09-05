# Runtime patch contract

The production loader is injected into the user-supplied APK and uses the
pinned ARM64 Dobby artifact. The LSPosed loader is development-only and calls
the same native hook core.

Before Unity starts, the high-priority provider establishes the bootstrap
boundary and the memorial launcher shows a localized startup log. The launcher
generates a 256-bit process capability, starts `libggfm_server.so` with the
application-private directory and Android `AssetManager`, begins the cold-login
session, then freezes the active USN for the process. It explicitly loads
`libil2cpp.so`, validates and installs every Hook, and only then opens the Unity
activity. A failure remains visible on this screen instead of crashing into the
game. Every original game request is redirected to the random loopback endpoint
and receives the capability, monotonic request sequence, device Unix time and
UTC offset headers.

The standalone loader also hooks libc `connect` before Unity loads. Internet
socket connections are accepted only for IPv4 `127.0.0.1` or IPv6 `::1`;
non-IP Unix-domain connections remain available to Android. A retired SDK that
is accidentally reached therefore cannot establish an external connection.

The settings screen reuses the original four rows and common popup. Its nested
pages cover one-item-per-mail legacy delivery, currency grants, all available
Star Pass seasons and save management. Unique content already owned or pending
in mail is rejected. A save or pass selection writes only pending state; the
next screen asks the player to restart and invokes Unity `Application.Quit`
only after explicit confirmation.

The bootstrap checks Server ABI 1 and compares Server's exported policy
SHA-256 with the Patch policy compiled into the loader. Startup fails closed on
either mismatch. SDK components for retired ads, billing, Play Games,
analytics and push are disabled by manifest transformation, while client call
sites that require callbacks are replaced by deterministic local flows.

For 8.0.0, `blueasa.FirebaseManagerSGT.Initialize` is also replaced by a
deterministic no-op. The original async path enters Firebase Messaging after
its Java component has been retired and aborts on a null JNI object. The hook
is guarded by the reviewed RVA and 16-byte prologue, so an unknown build fails
closed instead of receiving a speculative patch.

Firebase C++ also registers a Messaging initializer outside that managed path.
After the loopback boundary is active, the bootstrap preloads the pinned
original native library and replaces the shared implementation behind both
`firebase::messaging::Initialize` overloads with a successful local result
before Unity starts. This preserves the client's startup lifecycle without
restoring push, Analytics, Crashlytics, or any Firebase network service.

All six managed `FirebaseAnalyticsInternal.LogEvent` overloads are replaced by
local no-ops. The stock UI records telemetry before executing several button
actions; leaving those calls active after retiring Analytics raises
`internal::IsInitialized()` and interrupts the gameplay callback.
Before `FirebaseApp.Create`, it also invokes Firebase's exported
`SetEnabledAllAppCallbacks(false)` switch and suppresses the optional native
Crashlytics instance. Firebase Core remains available as the client expects,
but no retired App module is allowed to initialize.

Compatibility-manifest addresses are runtime ELF virtual addresses, never raw
file offsets. Patcher maps each RVA through the executable `PT_LOAD` segment
before checking bytes. This distinction matters for 8.0.0 because the ARM64
code segment has a `0x4000` virtual-to-file delta.
