# Guitar Girl Memorial Patch

Client-side patch implementation for the fan memorial build.

This repository contains only original patch code, manifests, and documentation.
It does not contain the game APK/XAPK, extracted assets, proprietary assemblies,
signing keys, or user save data.

## Responsibilities

- Redirect the retired network client to the built-in memorial server.
- Namespace every save-dependent client key by the active server USN.
- Expose the memorial settings entry and localized administration UI.
- Apply the documented quality-of-life balance overrides.
- Preserve content provenance: event and story rewards remain distinguishable.

Save-slot switching is deliberately two-phase. A requested slot becomes active
only after returning to the title/login flow and beginning a new session.

## Layout

- `bootstrap/`: the original Kotlin/Java bootstrap injected into the base APK.
- `native-core/`: one hook core shared by the standalone and LSPosed loaders.
- `compatibility/`: fail-closed hashes, RVAs, method descriptors and prologues.
- `policy/`: the versioned memorial rules consumed by Patch and Server.
- `localization/`: original memorial strings only; no copied game resources.

The standalone loader is the production path. The LSPosed adapter exists only
for development and calls the same `ggfm_install_hooks` entry point.

This repository intentionally does not vendor Dobby or the Rust server binary.
Release builds provide pinned, hash-verified artifacts to CMake. A missing or
mismatched dependency is a hard build failure.
