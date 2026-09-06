#!/usr/bin/env python3
"""Generate C++ hook tables from a verified compatibility manifest."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def _bytes(hex_value: str) -> str:
    raw = bytes.fromhex(hex_value)
    if len(raw) != 16:
        raise ValueError("every prologue must contain exactly 16 bytes")
    return ",".join(f"0x{value:02X}" for value in raw)


def _rva(value: str) -> int:
    parsed = int(value, 0)
    if parsed <= 0:
        raise ValueError("RVA must be positive")
    return parsed


def generate(manifest_path: Path) -> str:
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    hooks = manifest["il2cppHooks"]
    dependencies = manifest.get("il2cppDependencies", [])
    fields = manifest.get("il2cppFields", {})
    names = [entry["name"] for entry in hooks + dependencies]
    if len(names) != len(set(names)):
        raise ValueError("hook and dependency names must be unique")

    abi = manifest.get("source", {}).get("abi", "arm64-v8a")
    widths = {"arm64-v8a": 8, "armeabi-v7a": 4}
    if abi not in widths:
        raise ValueError(f"unsupported compatibility ABI: {abi}")
    lines = ["// Generated from compatibility manifest. Do not edit.",
             f'static_assert(sizeof(void*) == {widths[abi]}, "Compatibility manifest ABI does not match compiler target");']
    lines.append(f"constexpr std::array<HookTarget, {len(hooks)}> kGeneratedHookTargets = {{{{")
    for item in hooks:
        lines.append(
            f'    HookTarget{{"{item["name"]}", 0x{_rva(item["rva"]):X}, '
            f'std::array<std::uint8_t, 16>{{{_bytes(item["prologue"])}}}}},'
        )
    lines.append("}};")
    lines.append(
        f"constexpr std::array<FingerprintTarget, {len(dependencies)}> "
        "kGeneratedDependencies = {{"
    )
    for item in dependencies:
        lines.append(
            f'    FingerprintTarget{{"{item["name"]}", 0x{_rva(item["rva"]):X}, '
            f'std::array<std::uint8_t, 16>{{{_bytes(item["prologue"])}}}}},'
        )
    lines.append("}};")
    lines.append(
        f"constexpr std::array<GeneratedFieldOffset, {len(fields)}> "
        "kGeneratedFieldOffsets = {{"
    )
    for name, value in fields.items():
        lines.append(
            f'    GeneratedFieldOffset{{"{name}", 0x{_rva(value):X}}},'
        )
    lines.append("}};")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    content = generate(args.manifest)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(content, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
