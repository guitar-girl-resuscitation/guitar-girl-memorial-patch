#!/usr/bin/env python3
"""Generate resource-free UTF-8 C++ localization data for the native menu."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


KEYS = (
    "menu", "legacy", "currency", "pass", "saves", "close", "more", "back",
    "previous", "next", "send", "outfit", "guitar", "music", "ch1", "ch2",
    "about", "sent", "owned", "error", "restart", "create", "remove",
    "switch_save", "active", "pending", "about_description",
)


def cpp_utf8(value: str) -> str:
    return '"' + "".join(f"\\x{byte:02X}" for byte in value.encode("utf-8")) + '"'


def generate(path: Path) -> str:
    document = json.loads(path.read_text(encoding="utf-8"))
    locales = document["locales"]
    if document["fallback"] not in locales:
        raise ValueError("fallback locale is missing")
    expected = set(KEYS)
    for locale, values in locales.items():
        if set(values) != expected:
            raise ValueError(f"locale {locale} has incomplete or unknown keys")
    lines = ["// Generated from localization/strings.json. Do not edit."]
    lines.append(
        f"constexpr std::array<LocaleText, {len(locales)}> kGeneratedLocales = {{{{"
    )
    for locale, values in locales.items():
        fields = ", ".join(cpp_utf8(values[key]) for key in KEYS)
        lines.append(f"    LocaleText{{{cpp_utf8(locale)}, {fields}}},")
    lines.append("}};")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    main()
