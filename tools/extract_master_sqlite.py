"""Extract the shipped SQLite TextAsset from the user's table_db AssetBundle.

The deployment environment supplies a pinned UnityPy wheel. Neither the bundle
nor extracted database may be stored in this repository.
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import sqlite3
import tempfile
from dataclasses import dataclass
from pathlib import Path


SQLITE_MAGIC = b"SQLite format 3\x00"


@dataclass(frozen=True)
class SourceTable:
    asset_name: str
    source_name: str
    destination_name: str
    create_sql: str
    rows: list[tuple[object, ...]]


def _quoted(identifier: str) -> str:
    return '"' + identifier.replace('"', '""') + '"'


def _rewrite_create_table(create_sql: str, destination_name: str) -> str:
    pattern = re.compile(
        r"^\s*CREATE\s+TABLE\s+(?:IF\s+NOT\s+EXISTS\s+)?"
        r"(?:\"(?:\"\"|[^\"])+\"|`[^`]+`|\[[^]]+\]|[^\s(]+)",
        re.IGNORECASE,
    )
    replacement = f"CREATE TABLE {_quoted(destination_name)}"
    rewritten, count = pattern.subn(replacement, create_sql, count=1)
    if count != 1:
        raise RuntimeError(f"cannot rewrite CREATE TABLE statement: {create_sql!r}")
    return rewritten


def _read_source_tables(candidates: list[tuple[str, bytes]]) -> list[SourceTable]:
    occurrences: dict[str, list[str]] = {}
    raw_tables: list[tuple[str, str, str, list[tuple[object, ...]]]] = []
    for asset_name, raw_database in sorted(candidates):
        source = sqlite3.connect(":memory:")
        source.deserialize(raw_database)
        try:
            tables = source.execute(
                "SELECT name,sql FROM sqlite_master WHERE type='table' "
                "AND name NOT LIKE 'sqlite_%' AND name!='TableList' ORDER BY name"
            ).fetchall()
            for table_name, create_sql in tables:
                if not create_sql:
                    raise RuntimeError(f"master table {table_name} has no CREATE statement")
                quoted = _quoted(table_name)
                rows = source.execute(f"SELECT * FROM {quoted}").fetchall()
                occurrences.setdefault(table_name, []).append(asset_name)
                raw_tables.append((asset_name, table_name, create_sql, rows))
        finally:
            source.close()

    used_names: set[str] = set()
    result: list[SourceTable] = []
    # Prefer the source whose AssetBundle name matches the table name. The
    # shipped ChThirdChapter asset contains a typo-named ChThirdScore table;
    # this turns it back into ChThirdChapter without discarding either table.
    for asset_name, source_name, create_sql, rows in sorted(
        raw_tables,
        key=lambda item: (item[1], item[0] != item[1], item[0]),
    ):
        if len(occurrences[source_name]) == 1 and source_name not in used_names:
            destination_name = source_name
        elif asset_name not in used_names:
            destination_name = asset_name
        else:
            destination_name = f"{asset_name}__{source_name}"
        if destination_name in used_names:
            raise RuntimeError(
                f"cannot assign a unique destination table for {asset_name}/{source_name}"
            )
        used_names.add(destination_name)
        result.append(
            SourceTable(
                asset_name=asset_name,
                source_name=source_name,
                destination_name=destination_name,
                create_sql=create_sql,
                rows=rows,
            )
        )
    return result


def aggregate(candidates: list[tuple[str, bytes]], output: Path) -> None:
    if not candidates:
        raise RuntimeError("the bundle contains no SQLite TextAssets")
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise RuntimeError(f"refusing to overwrite {output}")

    source_tables = _read_source_tables(candidates)
    temporary_handle = tempfile.NamedTemporaryFile(
        prefix=f".{output.name}.", suffix=".tmp", dir=output.parent, delete=False
    )
    temporary_path = Path(temporary_handle.name)
    temporary_handle.close()
    try:
        destination = sqlite3.connect(temporary_path)
        try:
            destination.execute("PRAGMA journal_mode=DELETE")
            destination.execute("PRAGMA synchronous=FULL")
            destination.execute("BEGIN IMMEDIATE")
            destination.execute(
                "CREATE TABLE ggfm_master_schema("
                "version INTEGER NOT NULL, source_asset_count INTEGER NOT NULL, "
                "source_table_count INTEGER NOT NULL)"
            )
            destination.execute(
                "CREATE TABLE ggfm_master_assets("
                "asset_name TEXT PRIMARY KEY, sha256 TEXT NOT NULL, "
                "byte_length INTEGER NOT NULL)"
            )
            destination.execute(
                "CREATE TABLE ggfm_master_tables("
                "asset_name TEXT NOT NULL, source_table TEXT NOT NULL, "
                "destination_table TEXT PRIMARY KEY, row_count INTEGER NOT NULL, "
                "schema_sha256 TEXT NOT NULL, "
                "FOREIGN KEY(asset_name) REFERENCES ggfm_master_assets(asset_name))"
            )
            for asset_name, raw_database in sorted(candidates):
                destination.execute(
                    "INSERT INTO ggfm_master_assets VALUES(?,?,?)",
                    (
                        asset_name,
                        hashlib.sha256(raw_database).hexdigest().upper(),
                        len(raw_database),
                    ),
                )
            for table in source_tables:
                destination.execute(
                    _rewrite_create_table(table.create_sql, table.destination_name)
                )
                if table.rows:
                    placeholders = ",".join("?" for _ in table.rows[0])
                    destination.executemany(
                        f"INSERT INTO {_quoted(table.destination_name)} VALUES({placeholders})",
                        table.rows,
                    )
                destination.execute(
                    "INSERT INTO ggfm_master_tables VALUES(?,?,?,?,?)",
                    (
                        table.asset_name,
                        table.source_name,
                        table.destination_name,
                        len(table.rows),
                        hashlib.sha256(table.create_sql.encode("utf-8")).hexdigest().upper(),
                    ),
                )
            destination.execute(
                "INSERT INTO ggfm_master_schema VALUES(2,?,?)",
                (len(candidates), len(source_tables)),
            )
            destination.commit()
            integrity = destination.execute("PRAGMA integrity_check").fetchone()
            if integrity != ("ok",):
                raise RuntimeError(f"master.sqlite integrity check failed: {integrity}")
            destination.execute("VACUUM")
        finally:
            destination.close()
        os.replace(temporary_path, output)
    finally:
        temporary_path.unlink(missing_ok=True)


def extract(bundle: Path, output: Path) -> None:
    try:
        import UnityPy  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("a pinned UnityPy runtime is required") from error

    environment = UnityPy.load(str(bundle))
    candidates: list[tuple[str, bytes]] = []
    for obj in environment.objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        raw = data.m_Script
        # UnityPy exposes binary TextAssets through UTF-8 with surrogateescape
        # for this Unity version. This is the lossless inverse mapping.
        script = raw.encode("utf-8", "surrogateescape") if isinstance(raw, str) else bytes(raw)
        if script.startswith(SQLITE_MAGIC):
            candidates.append((str(data.m_Name), script))
    aggregate(candidates, output)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bundle", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    extract(args.bundle, args.output)


if __name__ == "__main__":
    main()
