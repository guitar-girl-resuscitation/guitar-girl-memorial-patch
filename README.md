# Guitar Girl Memorial Patch

English · [简体中文](README.zh-CN.md)

Client integration for **Guitar Girl Fan Memorial Build**. This repository contains our code and verified transformation rules, not a redistributed game or a prepatched APK.

The standalone build does **not** require Root or LSPosed. LSPosed is a development adapter for the shared native core, not the distribution format.

## The three repositories

| Repository | Responsibility |
| --- | --- |
| [Server](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-server) | Rust gameplay, protocol, SQLite saves and the Android server library |
| [Patch](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch) | Original client integration, memorial UI, identity isolation and verified transformation rules |
| [Patcher](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patcher) | CLI/web packaging, source verification, resource extraction, signing and downloads |

At runtime, the patched Unity client talks to the embedded Rust server over authenticated loopback. The patching website is **not** a game server and does not need to stay online for play.

## What Patch does

- Starts the embedded server before Unity, with a diagnostic launch screen and an explicit start button.
- Connects game requests to its dynamic local endpoint, supplying the session capability, monotonic request sequence and device clock.
- Namespaces save-dependent client preferences by USN. Install-wide settings such as language and volume remain separate.
- Integrates the memorial entry in game settings, localized announcements, legacy-item mail, resource grants, pass selection and save management.
- Applies client-side formulas from the same policy as Server so previews, costs and settlement behavior agree.
- Replaces retired SDK initialization/callback paths with local flows rather than requiring the original advertising, payment or online login infrastructure.
- Validates the supported build before applying any transformation.

The production runtime uses a standalone loader and pinned Dobby dependency. The LSPosed adapter shares the hook core; it must not develop a separate copy of gameplay rules.

## Player-facing behavior

Memorial settings expose lists of eligible discontinued clothes/guitars, ownership state and one-item-per-mail delivery. Already-owned or already-pending unique items cannot be sent again. Currency grants use whole-number input: likes, notes and fans use the game's reward multipliers; chocolate and candy use item counts. Reward bonuses follow the game's normal reward paths.

Pass selection lists seasons directly and resets that save's daily rotation anchor. Slot changes are pending until a restart/title-login boundary, never a live identity swap inside the current Unity session.

The launch screen provides diagnostics and save import/export controls. Import overwrites the current database: export anything you want to keep first. No compatibility with experimental/official cloud save formats is promised.

Memorial UI/announcements provide the client's 12 languages with English fallback. Real diagnostic logs intentionally remain English.

## Supported input and resource policy

Current compatibility target: **Guitar Girl 8.0.0, Android ARM64**, with exact outer XAPK SHA-256:

```text
E395AD8A0BF09EA9425D7751388D61C31E9B63411640A716432AC97940BB9FAC
```

An identical version label is not enough. [The compatibility manifest](compatibility/8.0.0.json) also checks required splits and relevant binary/data fingerprints. Unknown inputs fail closed; 7.0.0 is not an alternate supported source.

Public files may contain our implementations, hashes, method descriptors, offsets, short assertion patterns and structured transforms. They must not contain original game resources or full decompiled methods. Patcher derives master data from the user's package and modifies that private build locally.

## Repository map

| Path | Contents |
| --- | --- |
| `bootstrap/` | Original Android bootstrap, launch screen and adapters |
| `native-core/` | Shared native integration core and loaders |
| `compatibility/` | Build-specific verification and transformation manifests |
| `policy/` | Shared versioned balance policy |
| `localization/` | Original memorial translations |
| `tools/` | Runtime build, transform and validation tools |
| `tests/` | Static/contract regression tests |

## Build and validate

The release workflow is the reproducible toolchain reference: Java 21, Android platform 35 / build-tools 35.0.0, NDK 27.3.13750724, CMake 3.22.1, Python and Git. Release dependency resolution also uses GitHub CLI; CI supplies its token through the environment.

```sh
git clone https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch
cd guitar-girl-memorial-patch
python -m unittest discover -s tests -v
python tools/verify_policy.py policy/memorial-policy.v1.json
python tools/build_runtime.py --sdk "$ANDROID_HOME" --java-home "$JAVA_HOME" --server-tag nightly
```

The build resolves and verifies its Server and Dobby dependencies. Missing files, mismatched fingerprints or incompatible ABI/policy are hard failures. Dobby and the Server binary are not vendored.

Output is placed in `build/release-runtime/`, including `classes.dex`, native bootstrap/Dobby libraries and `dependencies.json`. The public [Patch release](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch/releases) packages runtime components as `ggfm-patch-android-arm64.zip`; **it cannot be installed as a game**.

To produce a game package, use [Patcher](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patcher) with the exact Patch checkout, its runtime archive and the Server artifact named in `dependencies.json`. Do not combine independently moving Nightlies.

## Policy and development workflow

[Memorial policy](policy/memorial-policy.v1.json) is versioned and fingerprinted on both sides. Change Server and client calculations together, add regression tests, then verify an actual patched build. Static method presence or a successful compilation is not proof of correct game behavior.

Changes to client persistence must preserve USN isolation, including Gallery, tutorial flags, chat and pending writes. A slot switch must terminate the prior session before new local keys become visible.

Successful main builds replace the single Nightly release; tags produce versioned releases. Preserve the exact commit, artifact digest and dependency record for reproducible packages.

## Further reading

- [Runtime integration and build contracts](docs/RUNTIME.md)
- [Client save-slot namespace](docs/CLIENT_SLOT_NAMESPACE.md)
- [Offline purchase order handling](docs/OFFLINE_PENDING_ORDERS.md)
- [Offline price queries](docs/OFFLINE_PRICE_QUERY.md)

## Scope, contributions and licensing

This is an unofficial fan memorial/interoperability project, not an official service or an endorsement by the original developers or publisher. It does not recover official accounts, cloud saves, payments or retired online services. Some historical server-only values are memorial compatibility choices, not a claim of complete original-server fidelity.

Project code is licensed under [AGPL-3.0-or-later](LICENSE); third-party components retain their own licenses. This does not license the original game. Supply only an original package you are entitled to use.

Do not submit APK/XAPK files, AssetBundles, original DEX/IL2CPP binaries, full decompiler exports, captured proprietary master tables, private saves or signing secrets. Report bugs with the component version/commit, chapter, reproducible steps and redacted diagnostic logs. For behavior changes, add a contract/regression test and keep both README languages in sync.
