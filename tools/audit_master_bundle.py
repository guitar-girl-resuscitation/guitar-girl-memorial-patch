"""Audit SQLite TextAssets in the shipped table AssetBundle.

This emits metadata only. It never writes or embeds the game's tables.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sqlite3
from collections import defaultdict
from pathlib import Path


SQLITE_MAGIC = b"SQLite format 3\x00"


def sqlite_assets(bundle: Path):
    try:
        import UnityPy  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("a pinned UnityPy runtime is required") from error

    for obj in UnityPy.load(str(bundle)).objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        raw_value = data.m_Script
        raw = (
            raw_value.encode("utf-8", "surrogateescape")
            if isinstance(raw_value, str)
            else bytes(raw_value)
        )
        if raw.startswith(SQLITE_MAGIC):
            yield str(data.m_Name), raw


def audit(bundle: Path) -> dict[str, object]:
    assets: list[dict[str, object]] = []
    table_sources: dict[str, list[dict[str, object]]] = defaultdict(list)
    for asset_name, raw_database in sorted(sqlite_assets(bundle)):
        source = sqlite3.connect(":memory:")
        source.deserialize(raw_database)
        try:
            tables = source.execute(
                "SELECT name,sql FROM sqlite_master "
                "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
                "AND name!='TableList' ORDER BY name"
            ).fetchall()
            asset_tables: list[dict[str, object]] = []
            for table_name, create_sql in tables:
                quoted = '"' + table_name.replace('"', '""') + '"'
                rows = source.execute(f"SELECT * FROM {quoted}").fetchall()
                digest = hashlib.sha256(repr(rows).encode("utf-8")).hexdigest().upper()
                entry = {
                    "name": table_name,
                    "row_count": len(rows),
                    "row_digest": digest,
                    "schema": create_sql,
                }
                asset_tables.append(entry)
                table_sources[table_name].append(
                    {
                        "asset": asset_name,
                        "row_count": len(rows),
                        "row_digest": digest,
                        "schema": create_sql,
                    }
                )
            assets.append(
                {
                    "name": asset_name,
                    "sha256": hashlib.sha256(raw_database).hexdigest().upper(),
                    "byte_length": len(raw_database),
                    "tables": asset_tables,
                }
            )
        finally:
            source.close()

    return {
        "source": str(bundle),
        "asset_count": len(assets),
        "table_name_count": len(table_sources),
        "duplicates": {
            name: entries for name, entries in sorted(table_sources.items()) if len(entries) > 1
        },
        "assets": assets,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--summary", action="store_true")
    args = parser.parse_args()
    report = audit(args.bundle)
    if args.summary:
        report = {
            "source": report["source"],
            "asset_count": report["asset_count"],
            "table_name_count": report["table_name_count"],
            "duplicates": report["duplicates"],
        }
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
