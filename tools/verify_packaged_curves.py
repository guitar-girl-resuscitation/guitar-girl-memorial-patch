"""Read-only, streaming QA of every cost row in private original/patched masters.

No fixtures or game rows are stored here. Run after extracting the actual output
XAPK, not just on the pre-packaging intermediate. Non-cost columns must be exact.
"""
import argparse
import math
import sqlite3


def verify(original, patched):
    source = sqlite3.connect(f"file:{original.as_posix()}?mode=ro", uri=True)
    output = sqlite3.connect(f"file:{patched.as_posix()}?mode=ro", uri=True)
    tables = [r[0] for r in source.execute("SELECT name FROM sqlite_master WHERE type='table'")
              if r[0].startswith(("CharacterCost", "Follower_", "MusicCost"))]
    counts = {prefix: sum(t.startswith(prefix) for t in tables)
              for prefix in ("CharacterCost", "Follower_", "MusicCost")}
    assert counts == {"CharacterCost": 2, "Follower_": 24, "MusicCost": 2}, counts
    total = 0
    try:
        for table in sorted(tables):
            q = '"' + table.replace('"', '""') + '"'
            columns = [r[1] for r in source.execute(f"PRAGMA table_info({q})")]
            cost = columns.index("d_Cost")
            digits = columns.index("i_CostDigits")
            before = source.execute(f"SELECT * FROM {q} ORDER BY rowid")
            after = output.execute(f"SELECT * FROM {q} ORDER BY rowid")
            count = 0
            for row in before:
                other = after.fetchone()
                if other is None: raise AssertionError(f"{table}: missing row {count}")
                for i, value in enumerate(row):
                    if i != cost:
                        assert value == other[i], (table, count, columns[i], "non-cost changed")
                expected = row[cost] * 0.1
                if row[cost] > 0 and row[digits] == 0:
                    expected = max(1, math.ceil(expected))
                assert math.isclose(other[cost], expected, rel_tol=2e-15, abs_tol=1e-9), (table,count,"cost not 0.1x")
                count += 1
            assert after.fetchone() is None, (table, "extra row")
            print(f"PASS {table}: {count} rows; costs 0.1x; digits/production unchanged")
            total += count
        print(f"PASS total: {len(tables)} tables, {total} rows")
    finally:
        source.close(); output.close()


if __name__ == "__main__":
    from pathlib import Path
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("patched", type=Path)
    args=parser.parse_args()
    verify(args.original.resolve(strict=True), args.patched.resolve(strict=True))
