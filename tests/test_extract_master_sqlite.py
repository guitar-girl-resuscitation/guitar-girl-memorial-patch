from __future__ import annotations

import importlib.util
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "tools" / "extract_master_sqlite.py"
SPEC = importlib.util.spec_from_file_location("extract_master_sqlite", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def database(table_name: str, columns: str, values: tuple[object, ...]) -> bytes:
    connection = sqlite3.connect(":memory:")
    connection.execute(f'CREATE TABLE "{table_name}"({columns})')
    connection.execute(
        f'INSERT INTO "{table_name}" VALUES({",".join("?" for _ in values)})', values
    )
    raw = connection.serialize()
    connection.close()
    return raw


class AggregateTests(unittest.TestCase):
    def test_duplicate_source_table_uses_asset_name_without_data_loss(self) -> None:
        candidates = [
            ("ChThirdChapter", database("ChThirdScore", "id INTEGER, name TEXT", (1, "chapter"))),
            ("ChThirdScore", database("ChThirdScore", "id INTEGER, score INTEGER", (2, 100))),
        ]
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "master.sqlite"
            MODULE.aggregate(candidates, output)
            destination = sqlite3.connect(output)
            self.assertEqual(
                destination.execute("SELECT * FROM ChThirdChapter").fetchall(),
                [(1, "chapter")],
            )
            self.assertEqual(
                destination.execute("SELECT * FROM ChThirdScore").fetchall(),
                [(2, 100)],
            )
            self.assertEqual(
                destination.execute("SELECT * FROM ggfm_master_schema").fetchall(),
                [(2, 2, 2)],
            )
            self.assertEqual(destination.execute("PRAGMA integrity_check").fetchone(), ("ok",))
            destination.close()


if __name__ == "__main__":
    unittest.main()
