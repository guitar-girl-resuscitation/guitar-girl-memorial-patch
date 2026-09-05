from __future__ import annotations

import importlib.util
import json
import sqlite3
import struct
import sys
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "tools" / "transform_master_bundle.py"
SPEC = importlib.util.spec_from_file_location("transform_master_bundle", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)
POLICY = json.loads(
    (Path(__file__).parents[1] / "policy" / "memorial-policy.v1.json").read_text(
        encoding="utf-8"
    )
)


def database(statements: list[str]) -> bytes:
    connection = sqlite3.connect(":memory:")
    for statement in statements:
        connection.execute(statement)
    raw = connection.serialize()
    connection.close()
    return raw


class MasterTransformTests(unittest.TestCase):
    def test_all_affection_thresholds_including_lily_remain_original(self):
        raw = database([
            'CREATE TABLE FollowerProfileLevel(i_Id INT,i_ProfileID INT,i_Level INT,d_RequireEXP REAL)',
            'INSERT INTO FollowerProfileLevel VALUES(1,1,1,0),(2,1,2,50),(3,1,3,150),(4,100000,2,100),(5,100000,3,300)',
        ])
        patched, report = MODULE.transform_database('FollowerProfileLevel', raw, POLICY)
        c = sqlite3.connect(':memory:'); self.addCleanup(c.close); c.deserialize(patched)
        self.assertEqual([r[0] for r in c.execute('SELECT d_RequireEXP FROM FollowerProfileLevel')], [0,50,150,100,300])
        self.assertEqual(report['FollowerProfileLevel.thresholdsPreserved'], 5)

    def test_relative_formula_caps_preserve_area_currency_and_original_expense(self):
        for table in ("Skill", "Unit"):
            raw = database([
                f"CREATE TABLE {table}(i_Id INT,i_Area INT,i_Cost INT,f_CostIncreaseValue REAL,i_MaxLevel INT,s_GoodsType TEXT,f_Cooltime REAL)",
                f"INSERT INTO {table} VALUES (1,1,100,10,20,'CP',300),(2,1,100,20,20,'CP',300),(3,1,100,30,20,'CP',300),"
                "(4,1,0,0,1,'CP',300),(5,2,500,100,20,'CP',300),(6,1,500,100,20,'Candy',300)",
            ])
            patched, _ = MODULE.transform_database(table, raw, POLICY)
            c = sqlite3.connect(":memory:"); self.addCleanup(c.close); c.deserialize(patched)
            f32 = lambda n: struct.unpack('<f', struct.pack('<f', n))[0]
            offset = 2 if table == 'Skill' else 1
            for id, base, step, maximum in c.execute(f"SELECT i_Id,i_Cost,f_CostIncreaseValue,i_MaxLevel FROM {table}"):
                prices = [int(f32(f32(base) + f32(f32(step)*max(0,target-offset)))) for target in range(1,maximum+1)]
                self.assertEqual(prices[-1], {1:10,2:15,3:20,4:0,5:10,6:10}[id])
                self.assertEqual(prices, sorted(prices))
                self.assertEqual(prices[0], 0 if id == 4 else 1)
                if id != 4:
                    self.assertGreater(len(set(prices)), 1, 'must not become a fixed-one price')

    def test_follower_cost_asset_contains_many_tables_not_one_asset_per_follower(self):
        raw = database([stmt for i in (1,6,7,16,201,208) for stmt in (
            f"CREATE TABLE Follower_{i}(i_Level INT,d_Cost REAL,i_CostDigits INT,d_Amount REAL)",
            f"INSERT INTO Follower_{i} VALUES(0,1000,0,50),(1,96.8832,18,80)")])
        patched, report = MODULE.transform_database("FollowerCost", raw, POLICY)
        c=sqlite3.connect(":memory:"); self.addCleanup(c.close); c.deserialize(patched)
        for i in (1,6,7,16,201,208):
            rows=c.execute(f"SELECT d_Cost,i_CostDigits,d_Amount FROM Follower_{i}").fetchall()
            self.assertEqual(rows[0], (100,0,50))
            self.assertAlmostEqual(rows[1][0],9.68832)
            self.assertEqual(rows[1][1:],(18,80))
            self.assertEqual(report[f"Follower_{i}.curveCostsScaled"],2)

    def test_skill_and_furniture_full_level_price_curves(self):
        for table in ("Skill", "Unit"):
            raw=database([f"CREATE TABLE {table}(i_Id INT,i_Cost INT,f_CostIncreaseValue REAL,i_MaxLevel INT,s_GoodsType TEXT,f_Cooltime REAL)",
                f"INSERT INTO {table} VALUES(1,100,10,20,'CP',300),(2,500,0,6,'Candy',300),(3,0,0,1,'CP',300)"])
            patched,_=MODULE.transform_database(table,raw,POLICY)
            c=sqlite3.connect(":memory:"); self.addCleanup(c.close); c.deserialize(patched)
            for base,increment,maximum in c.execute(f"SELECT i_Cost,f_CostIncreaseValue,i_MaxLevel FROM {table}"):
                if base==0:
                    self.assertEqual(increment,0); continue
                first = 2 if table == "Skill" else 1
                prices=[int(base+increment*max(0,target-first)) for target in range(first,maximum+1)]
                self.assertEqual(prices,sorted(prices)); self.assertEqual(prices[0],1); self.assertEqual(prices[-1],10)

    def test_cosmetic_progression_prices_keep_currency_and_exact_tenth(self):
        raw = database([
            "CREATE TABLE Costume(i_Id INT,i_Area INT,s_Cost TEXT,i_AcquisitionType INT,f_FanMultiply REAL,s_GoodsType TEXT)",
            "INSERT INTO Costume VALUES(1,1,'0',0,0,'GP')",
            "INSERT INTO Costume VALUES(2,1,'20568469772056800',0,0.1,'GP')",
            "INSERT INTO Costume VALUES(3,1,'500',0,0.1,'CP')",
            "INSERT INTO Costume VALUES(4,1,'150',0,0.1,'Candy')",
            "INSERT INTO Costume VALUES(201,2,'0',0,0,'GP')",
            "INSERT INTO Costume VALUES(202,2,'100000000000000000001',0,0.1,'GP2')",
            "INSERT INTO Costume VALUES(203,2,'0',1,0.1,'GP2')",
        ])
        patched, _ = MODULE.transform_database("Costume", raw, POLICY)
        c = sqlite3.connect(":memory:")
        self.addCleanup(c.close)
        c.deserialize(patched)
        self.assertEqual(c.execute("SELECT i_Id,s_Cost,s_GoodsType FROM Costume ORDER BY i_Id").fetchall(), [
            (1,'0','GP'), (2,'2056846977205680','GP'), (3,'10','CP'), (4,'10','Candy'),
            (201,'0','GP'), (202,'10000000000000000001','GP2'), (203,'0','GP2'),
        ])

    def test_gifts_have_distinct_existing_resource_names_without_changing_identity(self):
        raw = database([
            "CREATE TABLE FollowerGiftItem(i_Id INT,s_ResourceName TEXT,d_Value REAL)",
            *[f"INSERT INTO FollowerGiftItem VALUES({i},'icon_gift_gingerman',{20 if i == 1 else 30})" for i in range(1, 7)],
        ])
        patched, _ = MODULE.transform_database("FollowerGiftItem", raw, POLICY)
        c = sqlite3.connect(":memory:")
        self.addCleanup(c.close)
        c.deserialize(patched)
        expected = ["gingerman", "chocomilk", "sandwich", "bread", "grapefruit", "coffee"]
        self.assertEqual(c.execute("SELECT * FROM FollowerGiftItem ORDER BY i_Id").fetchall(),
                         [(i, "icon_gift_" + name, 20 if i == 1 else 30) for i, name in enumerate(expected, 1)])
        with self.assertRaisesRegex(RuntimeError, "precondition mismatch"):
            MODULE.transform_database("FollowerGiftItem", patched, POLICY)

    def test_memorial_shop_uses_each_stock_product_icon(self):
        expected = ["09", "10", "11", "12", "06", "07", "30", "05"]
        self.assertEqual([item["resourceName"] for item in POLICY["memorialShop"]["items"]],
                         ["icon_shop_" + value for value in expected])

    def test_only_total_likes_and_fan_count_are_scaled_in_both_chapters(self) -> None:
        ids = list(range(1, 11)) + list(range(201, 208)) + [999]
        raw = database([
            "CREATE TABLE Achievement(i_Id INT,s_Condition_1 TEXT,s_Condition_2 TEXT)",
            *[f"INSERT INTO Achievement VALUES({i},'1000000000000000000001','0')" for i in ids],
        ])
        patched, _ = MODULE.transform_database("Achievement", raw, POLICY)
        c = sqlite3.connect(":memory:")
        self.addCleanup(c.close)
        c.deserialize(patched)
        for i, threshold, empty in c.execute("SELECT * FROM Achievement"):
            expected = "100000000000000000001" if i in {3, 4, 203, 204} else "1000000000000000000001"
            self.assertEqual(threshold, expected, f"achievement {i}")
            self.assertEqual(empty, "0")

    def test_attendance_is_not_a_scaled_achievement(self) -> None:
        raw = database([
            "CREATE TABLE Achievement(i_Id INT,s_Condition_1 TEXT,s_Condition_2 TEXT)",
            "INSERT INTO Achievement VALUES(4,'1000','10000'),(10,'3','7')",
        ])
        patched, _ = MODULE.transform_database("Achievement", raw, POLICY)
        c=sqlite3.connect(":memory:"); c.deserialize(patched)
        self.assertEqual(c.execute("SELECT * FROM Achievement ORDER BY i_Id").fetchall(),
                         [(4,'100','1000'),(10,'3','7')])

    def test_skill_unlock_levels_are_not_upgrade_costs(self) -> None:
        raw=database([
            "CREATE TABLE Skill(i_Id INT,i_Cost INT,f_CostIncreaseValue REAL,f_Cooltime REAL,s_GoodsType TEXT,i_UnlockLevel INT,i_RequirementPropCount INT,i_MaxLevel INT)",
            "INSERT INTO Skill VALUES(2,100,2,3600,'CP',300,20,20)",
        ])
        patched,_=MODULE.transform_database("Skill",raw,POLICY)
        c=sqlite3.connect(":memory:"); c.deserialize(patched)
        self.assertEqual(c.execute("SELECT i_Cost,f_Cooltime,i_UnlockLevel,i_RequirementPropCount FROM Skill").fetchone(),
                         (1,360,300,20))

    def test_heart_price_curve_is_tenth_and_keeps_magnitude_and_free(self) -> None:
        raw = database(
            [
                "CREATE TABLE CharacterCost1(i_Level INT,d_Cost REAL,i_CostDigits INT,d_Amount REAL)",
                "INSERT INTO CharacterCost1 VALUES(0,0,0,1)",
                "INSERT INTO CharacterCost1 VALUES(1,100,0,2)",
                "INSERT INTO CharacterCost1 VALUES(2,200,3,3)",
                "INSERT INTO CharacterCost1 VALUES(3,300,6,4)",
            ]
        )
        transformed, _ = MODULE.transform_database("CharacterCost", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(
            connection.execute("SELECT d_Cost,i_CostDigits FROM CharacterCost1").fetchall(),
            [(0.0, 0), (10.0, 0), (20.0, 3), (30.0, 6)],
        )
        self.assertEqual(connection.execute("SELECT d_Amount FROM CharacterCost1").fetchall(),
                         [(1.0,), (2.0,), (3.0,), (4.0,)])

    def test_candy_furniture_curves_use_original_final_cost_and_preserve_free(self) -> None:
        raw = database([
            "CREATE TABLE PropLevel(i_PropId INT,i_Level INT,d_Cost REAL)",
            "INSERT INTO PropLevel VALUES(1,1,10),(1,2,20),(1,3,40),(2,1,100),(2,2,200),(2,3,350),(3,1,0)",
        ])
        transformed, _ = MODULE.transform_database("PropLevel", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(connection.execute("SELECT d_Cost FROM PropLevel").fetchall(),
                         [(1.0,), (5.0,), (10.0,), (1.0,), (10.0,), (20.0,), (0.0,)])

    def test_formula_costs_keep_progression_and_rank_each_premium_currency(self) -> None:
        raw = database([
            "CREATE TABLE Music(i_Id INT,s_GoodsType TEXT,d_Cost REAL,d_CostIncreaseValue REAL,d_Amount REAL)",
            "INSERT INTO Music VALUES(1,'GP',1000,1.5,99)",
            "INSERT INTO Music VALUES(2,'CP',100,2,98)",
            "INSERT INTO Music VALUES(3,'CP',200,3,97)",
            "INSERT INTO Music VALUES(4,'Candy',500,4,96)",
            "INSERT INTO Music VALUES(5,'CP',0,0,95)",
        ])
        transformed, _ = MODULE.transform_database("Music", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(connection.execute(
            "SELECT s_GoodsType,d_Cost,d_CostIncreaseValue,d_Amount FROM Music ORDER BY i_Id"
        ).fetchall(), [('GP',100.0,0.15,99.0), ('CP',1.0,0.0,98.0),
                     ('CP',10.0,0.0,97.0), ('Candy',1.0,0.0,96.0), ('CP',0.0,0.0,95.0)])

    def test_cosmetic_description_and_bonus_are_consistent_in_both_chapters(self) -> None:
        raw = database([
            "CREATE TABLE Guitar(i_Id INT,i_Area INT,s_Cost TEXT,i_AcquisitionType INT,d_IncrementValue REAL,s_GoodsType TEXT,s_Description_ZH_CHS TEXT,s_Description_EN TEXT)",
            "INSERT INTO Guitar VALUES(1,1,'0',0,1,'GP','默认','Default')",
            "INSERT INTO Guitar VALUES(2,1,'150',0,1.05,'Candy','所有赞增加5%','Likes +5%')",
            "INSERT INTO Guitar VALUES(3,1,'0',3,1.1,'NotForSale','所有赞增加10%','Likes +10%')",
            "INSERT INTO Guitar VALUES(201,2,'0',0,1,'GP','默认','Default')",
            "INSERT INTO Guitar VALUES(202,2,'150',0,1.15,'CP','所有爱心增加15%','Hearts +15%')",
            "INSERT INTO Guitar VALUES(203,2,'0',3,1.25,'NotForSale','所有爱心增加25%','Hearts +25%')",
        ])
        transformed, _ = MODULE.transform_database("Guitar", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(connection.execute(
            "SELECT s_Cost,i_AcquisitionType,d_IncrementValue,s_GoodsType,s_Description_EN FROM Guitar ORDER BY i_Id"
        ).fetchall(), [('0',0,1.0,'GP','Default'), ('10',0,1.2,'Candy','Likes +20%'),
                      ('0',3,1.2,'NotForSale','Likes +20%'), ('0',0,1.0,'GP','Default'),
                      ('10',0,1.25,'CP','Hearts +25%'), ('0',3,1.25,'NotForSale','Hearts +25%')])
        self.assertEqual(MODULE.rewrite_percentage('[000000]Lv.20: +5%, 10 % / 2.5％', 30),
                         '[000000]Lv.20: +30%, 30 % / 30％')

    def test_fan_first_grade_is_preserved_and_large_decimal_is_exact(self) -> None:
        raw = database(
            [
                "CREATE TABLE Fan(i_Area INT,i_Grade INT,i_FanCount INT)",
                "INSERT INTO Fan VALUES(1,1,1000)",
                "INSERT INTO Fan VALUES(1,2,10001)",
            ]
        )
        transformed, _ = MODULE.transform_database("Fan", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(
            connection.execute("SELECT i_FanCount FROM Fan ORDER BY i_Grade").fetchall(),
            [(1000,), (1901,)],
        )
        self.assertEqual(
            MODULE.scaled_decimal("1000000000000000000000000000000000001", MODULE.Decimal("0.1")),
            "100000000000000000000000000000000001",
        )
        self.assertEqual(MODULE.scaled_decimal(0, MODULE.Decimal("0.1")), 0)

    def test_sqlite_library_version_is_canonicalized_to_source(self) -> None:
        source = bytearray(database(["CREATE TABLE Example(value INT)"]))
        transformed = bytearray(source)
        source[96:100] = b"SRC!"
        transformed[96:100] = b"HOST"
        normalized = MODULE.canonicalize_sqlite_header(bytes(source), bytes(transformed))
        self.assertEqual(normalized[96:100], b"SRC!")

    def test_skill_reset_is_five_minutes_but_other_cooldowns_are_tenth(self) -> None:
        raw = database(
            [
                "CREATE TABLE Skill(i_Id INT,i_Cost INT,f_CostIncreaseValue REAL,f_Cooltime REAL,s_GoodsType TEXT,i_MaxLevel INT)",
                "INSERT INTO Skill VALUES(1,100,10,1800,'CP',20)",
                "INSERT INTO Skill VALUES(4,0,0,900,'CP',1)",
            ]
        )
        transformed, _ = MODULE.transform_database("Skill", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(
            connection.execute("SELECT i_Cost,f_CostIncreaseValue,f_Cooltime FROM Skill ORDER BY i_Id").fetchall(),
            [(1, 0.5, 180.0), (0, 0.0, 300.0)],
        )

    def test_memorial_shop_rows_use_direct_server_purchase_state(self) -> None:
        raw = database(
            [
                "CREATE TABLE Shop("
                "i_Id INT,s_ProductID_ios TEXT,s_ProductID_aos TEXT,i_ShopCategory INT,"
                "i_ProductType INT,s_ResourceName TEXT,s_Title_EN TEXT,s_Title_ZH_CHS TEXT,"
                "s_Description_EN TEXT,s_Description_ZH_CHS TEXT,i_RewardGroup INT,"
                "i_SellType INT,i_SellValue INT,b_IsActive INT,i_SortIndex INT,s_AreaList TEXT)"
            ]
        )
        transformed, report = MODULE.transform_database("Shop", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(report["Shop.memorialRowsAppended"], 8)
        self.assertEqual(
            connection.execute(
                "SELECT i_Id,i_ProductType,i_SellType,i_SellValue,b_IsActive,i_RewardGroup "
                "FROM Shop WHERE i_Id=50001"
            ).fetchone(),
            (50001, 4, 0, 0, 1, 95001),
        )
        self.assertEqual(
            connection.execute("SELECT s_Title_ZH_CHS FROM Shop WHERE i_Id=50008").fetchone(),
            ("连续点击",),
        )

    def test_memorial_shop_reward_groups_are_projected(self) -> None:
        raw = database(
            [
                "CREATE TABLE RewardGroup(i_Id INT,i_Group INT,i_RewardType INT,"
                "i_RewardID INT,l_RewardQuantity REAL,i_BuyFirstQuantity INT)"
            ]
        )
        transformed, report = MODULE.transform_database("RewardGroup", raw, POLICY)
        connection = sqlite3.connect(":memory:")
        connection.deserialize(transformed)
        self.assertEqual(report["RewardGroup.memorialRowsAppended"], 11)
        self.assertEqual(
            connection.execute(
                "SELECT i_RewardType,i_RewardID,l_RewardQuantity FROM RewardGroup "
                "WHERE i_Group=95006 ORDER BY i_Id"
            ).fetchall(),
            [(1, 1, 12000.0), (1, 2, 1000000.0), (6, 4, 1.0)],
        )


if __name__ == "__main__":
    unittest.main()
