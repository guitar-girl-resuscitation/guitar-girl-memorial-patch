# ARM64 hook boundary contract

The pinned client contains tightly packed methods. A 16-byte fingerprint is an
identity check, not permission to overwrite all 16 bytes.

The settings `OnDisable` entry at RVA `0x1E1CB60` is a four-byte tail branch.
The English selection handler starts at `0x1E1CB64`; other settings callbacks
follow immediately. The hook now targets the disable implementation body at
`0x1E1C368`, preserving those neighboring entry points.

Several endpoint getters are eight-byte methods. The standalone Dobby adapter
must explicitly enable its ARM64 near-branch plugin; compiling that plugin into
Dobby alone does not activate it. Game hooks may change only the first four-byte
instruction. After each install the shared core checks that bytes 4 through 15
are unchanged. A fallback long jump is a startup failure, never a usable build.

Regression acceptance includes switching language, restarting with that language,
opening settings repeatedly, and checking all endpoint getter hooks. The runtime
guard complements version fingerprints; it does not replace source-package hashes.

The pinned Dobby allocator is corrected by `tools/harden_dobby.py` during every
runtime build. Both input files are SHA-256 checked (normalized LF) before either
is changed, and their originals are retained outside release payloads. Address
hints must not replace existing mappings: a different `mmap` result is released
and reported as allocation failure. The fallback that searches zero-filled bytes
in unrelated readable/executable mappings is disabled. No live mapping owned by
Unity, ART or another library is available as trampoline storage.

These are preventive memory-safety corrections. An intermittent diagnostic
startup crash motivated the inspection, but a passing build alone does not prove
that this was its cause or that repeated cold-start acceptance is complete.

This document contains authored compatibility descriptions only, not game code.
