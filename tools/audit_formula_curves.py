"""Read-only audit: transform private original Skill/Unit tables in memory.

No source assets, databases or player state are modified. Integer price output
uses the audited client float32 operations rather than Python float64 arithmetic.
"""
from __future__ import annotations

import argparse
import json
import sqlite3
import struct
from pathlib import Path

from transform_master_bundle import formula_price_caps, transform_formula_costs, transform_prop_level_costs


def f32(n: float) -> float:
    return struct.unpack('<f', struct.pack('<f', n))[0]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('original', type=Path)
    args = parser.parse_args()
    source = sqlite3.connect(args.original.resolve().as_uri() + '?mode=ro', uri=True)
    target = sqlite3.connect(':memory:')
    policy = json.loads((Path(__file__).parents[1] / 'policy/memorial-policy.v1.json').read_text(encoding='utf-8'))
    try:
        # Copy only the two tiny formula tables, not the full cost master.
        for table in ('Skill', 'Unit'):
            schema = source.execute('SELECT sql FROM sqlite_master WHERE name=?', (table,)).fetchone()[0]
            target.execute(schema)
            rows = source.execute(f'SELECT * FROM {table}').fetchall()
            target.executemany(f'INSERT INTO {table} VALUES ({",".join("?" for _ in rows[0])})', rows)
            caps = formula_price_caps(target, table, policy)
            transform_formula_costs(target, table, policy, {})
            offset = 2 if table == 'Skill' else 1
            for rowid, id, name, base, step, maximum in target.execute(
                f'SELECT rowid,i_Id,s_Name_ZH_CHS,i_Cost,f_CostIncreaseValue,i_MaxLevel FROM {table}'
            ):
                prices = [int(f32(f32(base) + f32(f32(step) * max(0, level-offset)))) for level in range(1, maximum+1)]
                assert prices[-1] == caps.get(rowid, 0), (table, id, prices)
                assert prices == sorted(prices), (table, id, prices)
                if base > 0 and maximum > offset:
                    assert prices[-1] > prices[0], (table, id, 'fixed price')
                print(f'{table} {id} {name}: target levels 1..{maximum}: {prices}')
        table = 'PropLevel'
        target.execute(source.execute('SELECT sql FROM sqlite_master WHERE name=?', (table,)).fetchone()[0])
        rows = source.execute(f'SELECT * FROM {table}').fetchall()
        target.executemany(f'INSERT INTO {table} VALUES ({",".join("?" for _ in rows[0])})', rows)
        transform_prop_level_costs(target, policy, {})
        for id, name in source.execute('SELECT i_Id,s_Name_ZH_CHS FROM Prop ORDER BY i_Id'):
            prices = [int(row[0]) for row in target.execute('SELECT d_Cost FROM PropLevel WHERE i_PropId=? ORDER BY i_Level', (id,))]
            assert prices == sorted(prices), (id, prices)
            assert prices[0] == 1 and 10 <= prices[-1] <= 20, (id, prices)
            print(f'Prop {id} {name} Candy: {prices}')
    finally:
        source.close()
        target.close()


if __name__ == '__main__':
    main()
