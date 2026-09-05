"""Rebuild cache identities, CRCs and size tables from verified user assets.

No Unity assets are shipped with this tool. Call only after split verification.
The 128-bit cache identifier is derived from SHA-256 of uncompressed content;
it is not claimed to be Unity Editor's build hash. CRC remains a real CRC-32.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import zipfile
import zlib

PREFIX = "assets/AssetBundles/Android/"
TABLE = "table/table_db.ab"
SIZES = "table/table_bundlesize.ab"


def content_identity(payload: bytes) -> tuple[int, bytes]:
    import UnityPy
    environment = UnityPy.load(payload)
    # The pinned three bundles each have one serialized CAB with no resource
    # sidecar. Read the actual decompressed bytes, never reserialize for CRC.
    if len(environment.file.files) != 1:
        raise ValueError("expected one serialized CAB")
    cab = next(iter(environment.file.files.values()))
    if not hasattr(cab, "reader"):
        raise ValueError("missing serialized CAB reader")
    raw = cab.reader.bytes
    return zlib.crc32(raw), hashlib.sha256(raw).digest()[:16]


def replace_once(pattern: str, replacement: str, text: str) -> str:
    result, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
    if count != 1:
        raise ValueError(f"expected exactly one metadata target, found {count}")
    return result


def rewrite_manifest(original: bytes, old_bundle: bytes, new_bundle: bytes,
                     *, verify_source_crc: bool = True) -> bytes:
    text = original.decode("utf-8")
    old_crc, _ = content_identity(old_bundle)
    match = re.search(r"^CRC: (\d+)\s*$", text, re.MULTILINE)
    if match is None or (verify_source_crc and int(match[1]) != old_crc):
        raise ValueError("source manifest CRC does not match verified source bundle")
    crc, identity = content_identity(new_bundle)
    text = replace_once(r"^CRC: \d+\s*$", f"CRC: {crc}", text)
    # Root Android.manifest has no Hashes section; child manifests do.
    if "AssetFileHash:" in text:
        text = replace_once(
            r"(  AssetFileHash:\r?\n    serializedVersion: \d+\r?\n    Hash: )[0-9a-fA-F]+",
            r"\g<1>" + identity.hex(), text,
        )
    return text.encode("utf-8")


def rewrite_size_text(text: str, table_size: int, sizes_size: int) -> str:
    for name, size in ((TABLE, table_size), (SIZES, sizes_size)):
        text = replace_once(r"^" + re.escape(name) + r" \d+$", f"{name} {size}", text)
    return text


def rewrite_sizes(source: bytes, table_size: int) -> bytes:
    import UnityPy
    environment = UnityPy.load(source)
    matches = [o for o in environment.objects if o.type.name == "TextAsset"
               and o.read().m_Name == "Android_SizeTable"]
    if len(matches) != 1:
        raise ValueError("expected one Android size table")
    data = matches[0].read()
    original = data.m_Script
    # Uncompressed output makes the self-size equation stable: only the
    # number of decimal digits can change the serialized length.
    expected_size = len(source)
    for _ in range(8):
        data.m_Script = rewrite_size_text(original, table_size, expected_size)
        data.save()
        payload = environment.file.save(packer="none")
        if len(payload) == expected_size:
            return payload
        expected_size = len(payload)
    raise ValueError("size table self-size failed to converge")


def rewrite_root(source: bytes, identities: dict[str, bytes]) -> bytes:
    import UnityPy
    environment = UnityPy.load(source)
    objects = [o for o in environment.objects if o.type.name == "AssetBundleManifest"]
    if len(objects) != 1:
        raise ValueError("expected one root AssetBundleManifest")
    obj = objects[0]
    tree = obj.read_typetree()
    names = dict(tree["AssetBundleNames"])
    changed = set()
    for row_id, info in tree["AssetBundleInfos"]:
        name = names[row_id]
        if name not in identities:
            continue
        info["AssetBundleHash"] = {
            f"bytes[{i}]": value for i, value in enumerate(identities[name])
        }
        changed.add(name)
    if changed != identities.keys():
        raise ValueError("root manifest lacks a changed bundle")
    obj.save_typetree(tree)
    return environment.file.save(packer="original")


def transform(source_apk: Path, table_bundle: Path, output: Path) -> None:
    if output.exists():
        raise FileExistsError(output)
    with zipfile.ZipFile(source_apk) as archive:
        def read(name: str) -> bytes:
            member = archive.getinfo(PREFIX + name)
            if member.file_size > 32 * 1024 * 1024:
                raise ValueError("metadata input exceeds limit")
            return archive.read(member)
        original_table, original_sizes, original_root = read(TABLE), read(SIZES), read("Android")
        table = table_bundle.read_bytes()
        sizes = rewrite_sizes(original_sizes, len(table))
        identities = {TABLE: content_identity(table)[1], SIZES: content_identity(sizes)[1]}
        root = rewrite_root(original_root, identities)
        outputs = {
            "Android": root,
            # In the hash-verified 8.0.0 split, the original root's text CRC
            # disagrees with its actual CAB (child CRCs do agree). Outer/split
            # fingerprints pin both inputs. Emit the actual new root CRC;
            # never propagate this original inconsistent declaration.
            "Android.manifest": rewrite_manifest(read("Android.manifest"), original_root, root,
                                                  verify_source_crc=False),
            TABLE + ".manifest": rewrite_manifest(read(TABLE + ".manifest"), original_table, table),
            SIZES: sizes,
            SIZES + ".manifest": rewrite_manifest(read(SIZES + ".manifest"), original_sizes, sizes),
        }
    output.mkdir(parents=True)
    for name, payload in outputs.items():
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open("xb") as stream:
            stream.write(payload)
    print(f"Metadata consistent: table={len(table)} bytes; sizes={len(sizes)} bytes; files={len(outputs)}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("source_apk", type=Path)
    parser.add_argument("table_bundle", type=Path)
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()
    transform(arguments.source_apk, arguments.table_bundle, arguments.output)
