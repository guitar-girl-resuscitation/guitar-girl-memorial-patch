"""Apply GGFM policy transforms to the user's own table AssetBundle.

No game rows live in this repository.  The input bundle must already have
passed the compatibility-manifest hashes; this tool additionally checks its
exact SHA-256 and every table/column precondition before producing a new
bundle.  The output is created without overwriting an existing file.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import sqlite3
import struct
import tempfile
from decimal import Decimal, ROUND_CEILING, localcontext
from pathlib import Path
from typing import Any


SQLITE_MAGIC = b"SQLite format 3\x00"
COST_CURVE_ASSETS = {"CharacterCost", "MusicCost", "FollowerCost"}
COST_CURVE_PREFIXES = ("Follower_",)
FORMULA_COST_TABLES = {"Character", "Follower", "Music", "Skill", "Unit"}
LEVEL_REQUIREMENT_COLUMNS = {
    "i_UnlockLevel",
    "i_RequirementCharacterLevel",
    "i_RequirementFollowerLevel",
    "i_RequirementPropCount",
    "d_UnlockCost",
    "i_UnlockCost",
}


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest().upper()


def quote(identifier: str) -> str:
    return '"' + identifier.replace('"', '""') + '"'


def columns(connection: sqlite3.Connection, table: str) -> set[str]:
    return {str(row[1]) for row in connection.execute(f"PRAGMA table_info({quote(table)})")}


def require_columns(connection: sqlite3.Connection, table: str, required: set[str]) -> None:
    missing = required - columns(connection, table)
    if missing:
        raise RuntimeError(f"{table} is missing policy columns: {sorted(missing)}")


def scaled_decimal(value: Any, multiplier: Decimal) -> Any:
    if value is None:
        return value
    source_text = str(value)
    with localcontext() as context:
        context.prec = max(64, len(source_text) + 16)
        decimal = Decimal(source_text)
        if decimal <= 0:
            return value
        scaled = max(
            Decimal(1),
            (decimal * multiplier).to_integral_value(rounding=ROUND_CEILING),
        )
    if isinstance(value, str):
        return format(scaled, "f")
    if isinstance(value, int):
        return int(scaled)
    return float(scaled)


def canonicalize_sqlite_header(source: bytes, transformed: bytes) -> bytes:
    """Remove the build host's SQLite library version from deterministic output.

    SQLite stores its library version at header bytes 96..100 whenever a
    database is modified.  The field has no gameplay semantics, so preserve
    the audited source value instead of leaking whichever Python runtime the
    patcher deployment happens to use.
    """
    if not source.startswith(SQLITE_MAGIC) or not transformed.startswith(SQLITE_MAGIC):
        raise RuntimeError("cannot canonicalize a non-SQLite payload")
    if len(source) < 100 or len(transformed) < 100:
        raise RuntimeError("SQLite payload is shorter than its fixed header")
    result = bytearray(transformed)
    result[96:100] = source[96:100]
    return bytes(result)


def rank_map(values: list[Any]) -> dict[Any, int]:
    distinct = sorted(
        {value for value in values if value is not None and float(value) > 0},
        key=lambda value: Decimal(str(value)),
    )
    if not distinct:
        return {}
    if len(distinct) == 1:
        return {distinct[0]: 1}
    return {
        value: 1 + (9 * rank // (len(distinct) - 1))
        for rank, value in enumerate(distinct)
    }


def map_positive_column(
    connection: sqlite3.Connection, table: str, column: str, report: dict[str, int]
) -> None:
    rows = connection.execute(f"SELECT rowid,{quote(column)} FROM {quote(table)}").fetchall()
    mapping = rank_map([value for _, value in rows])
    changed = 0
    for rowid, original in rows:
        if original not in mapping:
            continue
        cursor = connection.execute(
            f"UPDATE {quote(table)} SET {quote(column)}=? WHERE rowid=?",
            (mapping[original], rowid),
        )
        changed += cursor.rowcount
    report[f"{table}.{column}.rankMapped"] = changed


def scale_curve_costs(connection: sqlite3.Connection, table: str,
                      multiplier: Decimal, report: dict[str, int]) -> None:
    # d_Cost is a mantissa; i_CostDigits carries the magnitude. Clearing that
    # exponent turns late-game costs into single digits and destroys the curve.
    require_columns(connection, table, {"d_Cost", "i_CostDigits"})
    rows = connection.execute(
        f"SELECT rowid,d_Cost,i_CostDigits FROM {quote(table)}"
    ).fetchall()
    changed = 0
    for rowid, cost, digits in rows:
        if cost is None or cost <= 0:
            continue
        scaled = Decimal(str(cost)) * multiplier
        if digits == 0:
            scaled = max(Decimal(1), scaled.to_integral_value(rounding=ROUND_CEILING))
        connection.execute(f"UPDATE {quote(table)} SET d_Cost=? WHERE rowid=?",
                           (float(scaled), rowid))
        changed += 1
    report[f"{table}.curveCostsScaled"] = changed


def formula_price_caps(connection: sqlite3.Connection, table: str,
                       policy: dict[str, Any]) -> dict[int, int]:
    """Compare original final prices within a system/area/currency before editing rows."""
    available = columns(connection, table)
    require_columns(connection, table, {"i_MaxLevel", "s_GoodsType"})
    cost = "i_Cost" if "i_Cost" in available else "d_Cost"
    increment = next((c for c in ("f_CostIncreaseValue", "d_CostIncreaseValue") if c in available), None)
    if increment is None:
        raise RuntimeError(f"{table} is missing its level price increment")
    area = quote("i_Area") if "i_Area" in available else "1"
    groups: dict[tuple[int, str], list[tuple[int, float]]] = {}
    f32 = lambda n: struct.unpack("<f", struct.pack("<f", n))[0]
    offset = 2 if table == "Skill" else 1
    for rowid, base, step, maximum, region, goods in connection.execute(
        f"SELECT rowid,{quote(cost)},{quote(increment)},i_MaxLevel,{area},s_GoodsType FROM {quote(table)}"
    ):
        if base > 0:
            final = int(f32(f32(base) + f32(f32(step) * max(0, maximum-offset))))
            groups.setdefault((region, str(goods).casefold()), []).append((rowid, final))
    low = int(policy["upgradeCosts"]["formulaCapMin"])
    high = int(policy["upgradeCosts"]["formulaCapMax"])
    caps = {}
    for entries in groups.values():
        minimum = min(price for _, price in entries)
        maximum = max(price for _, price in entries)
        for rowid, price in entries:
            caps[rowid] = low if maximum == minimum else low + math.floor(
                (price-minimum)/(maximum-minimum)*(high-low)+0.5)
    return caps


def transform_prop_level_costs(connection: sqlite3.Connection, policy: dict[str, Any],
                               report: dict[str, int]) -> None:
    require_columns(connection, 'PropLevel', {'i_PropId', 'i_Level', 'd_Cost'})
    rows = connection.execute('SELECT rowid,i_PropId,i_Level,d_Cost FROM PropLevel').fetchall()
    limits: dict[int, tuple[int, float]] = {}
    for _, id, level, cost in rows:
        old = limits.get(id, (0, 0.0))
        limits[id] = (max(old[0], level), max(old[1], cost))
    positive = [cost for _, cost in limits.values() if cost > 0]
    if not positive:
        return
    minimum, maximum = min(positive), max(positive)
    low, high = policy['upgradeCosts']['formulaCapMin'], policy['upgradeCosts']['formulaCapMax']
    f32 = lambda n: struct.unpack('<f', struct.pack('<f', n))[0]
    for rowid, id, level, cost in rows:
        if cost <= 0:
            continue
        maxlevel, final = limits[id]
        cap = low if minimum == maximum else low + math.floor((final-minimum)/(maximum-minimum)*(high-low)+0.5)
        step = f32((cap-1)/(maxlevel-1)) if maxlevel > 1 else 0.0
        price = int(f32(1.0+f32(step*max(0, level-1))))
        connection.execute('UPDATE PropLevel SET d_Cost=? WHERE rowid=?', (price, rowid))
    report['PropLevel.levelCurveCosts'] = len(rows)


def transform_formula_costs(connection: sqlite3.Connection, table: str,
                            policy: dict[str, Any], report: dict[str, int]) -> None:
    available = columns(connection, table)
    cost_column = "i_Cost" if "i_Cost" in available else "d_Cost"
    require_columns(connection, table, {cost_column})
    currency_column = "s_GoodsType" if "s_GoodsType" in available else None
    if currency_column is None and table not in {"Character", "Follower"}:
        raise RuntimeError(f"{table} is missing its upgrade currency")
    currency_sql = quote(currency_column) if currency_column else "'GP'"
    rows = connection.execute(
        f"SELECT rowid,{quote(cost_column)},{currency_sql} FROM {quote(table)}"
    ).fetchall()
    premium = {value.casefold() for value in policy["upgradeCosts"]["singleDigitCurrencies"]}
    mappings = {
        currency: rank_map([cost for _, cost, goods in rows if str(goods).casefold() == currency])
        for currency in premium
    }
    increments = [column for column in ("f_CostIncreaseValue", "d_CostIncreaseValue")
                  if column in available]
    multiplier = Decimal(str(policy["progressionMultiplier"]))
    caps = formula_price_caps(connection, table, policy) if table in {"Skill", "Unit"} else {}
    for rowid, cost, goods in rows:
        currency = str(goods).casefold()
        level_curve = table in {"Skill", "Unit"} and currency in premium
        if level_curve:
            require_columns(connection, table, {"i_MaxLevel"})
            maximum = connection.execute(
                f"SELECT i_MaxLevel FROM {quote(table)} WHERE rowid=?", (rowid,)
            ).fetchone()[0]
        replacement = (mappings[currency].get(cost, cost) if currency in premium
                       else float(Decimal(str(cost)) * multiplier) if isinstance(cost, float)
                       else scaled_decimal(cost, multiplier))
        if level_curve:
            replacement = 1 if cost > 0 else 0
        connection.execute(f"UPDATE {quote(table)} SET {quote(cost_column)}=? WHERE rowid=?",
                           (replacement, rowid))
        for column in increments:
            original = connection.execute(
                f"SELECT {quote(column)} FROM {quote(table)} WHERE rowid=?", (rowid,)
            ).fetchone()[0]
            replacement = (0 if currency in premium
                           else float(Decimal(str(original)) * multiplier) if isinstance(original, float)
                           else scaled_decimal(original, multiplier))
            if level_curve:
                # Client: skill starts at target level 2; furniture at level 1.
                # The final paid upgrade targets maxLevel, not maxLevel+1.
                first = 2 if table == "Skill" else 1
                replacement = ((caps[rowid]-1.0) / (maximum - first) if cost > 0 and maximum > first else 0.0)
            connection.execute(f"UPDATE {quote(table)} SET {quote(column)}=? WHERE rowid=?",
                               (replacement, rowid))
    report[f"{table}.currencyScopedCosts"] = len(rows)


def rewrite_percentage(text: str, percent: int) -> str:
    return re.sub(r"[0-9]+(?:\.[0-9]+)?(?=\s*[%％])", str(percent), text)


def scale_columns(
    connection: sqlite3.Connection,
    table: str,
    selected: list[str],
    multiplier: Decimal,
    report: dict[str, int],
    included_ids: tuple[int, ...] | None = None,
) -> None:
    for column in selected:
        selection = "" if included_ids is None else (
            " WHERE i_Id IN (" + ",".join("?" for _ in included_ids) + ")"
            if included_ids else " WHERE 0"
        )
        rows = connection.execute(
            f"SELECT rowid,{quote(column)} FROM {quote(table)}" + selection, included_ids or ()
        ).fetchall()
        changed = 0
        for rowid, original in rows:
            if original is None:
                continue
            replacement = scaled_decimal(original, multiplier)
            if replacement != original:
                connection.execute(
                    f"UPDATE {quote(table)} SET {quote(column)}=? WHERE rowid=?",
                    (replacement, rowid),
                )
                changed += 1
        report[f"{table}.{column}.scaled"] = changed


def transform_cosmetics(
    connection: sqlite3.Connection,
    table: str,
    price: int,
    multiplier: Decimal,
    report: dict[str, int],
) -> None:
    if table == "Costume":
        bonus_column = "f_FanMultiply"
    else:
        bonus_column = "d_IncrementValue"
    require_columns(
        connection,
        table,
        {"i_Id", "i_Area", "s_Cost", "s_GoodsType", "i_AcquisitionType", bonus_column},
    )
    priced = 0
    for rowid, goods, official in connection.execute(
        f"SELECT rowid,s_GoodsType,s_Cost FROM {quote(table)} "
        "WHERE i_AcquisitionType=0 AND CAST(s_Cost AS TEXT) NOT IN ('', '0')"
    ).fetchall():
        if goods.upper() in {"GP", "GP2"}:
            replacement = scaled_decimal(official, multiplier)
        elif goods.upper() in {"CP", "CANDY"}:
            replacement = str(price)
        else:
            raise RuntimeError(f"unsupported purchasable {table} currency: {goods}")
        connection.execute(f"UPDATE {quote(table)} SET s_Cost=? WHERE rowid=?", (replacement, rowid))
        priced += 1
    report[f"{table}.purchasablePrice"] = priced
    areas = [row[0] for row in connection.execute(f"SELECT DISTINCT i_Area FROM {quote(table)}")]
    bonus_changes = 0
    for area in areas:
        default_id = connection.execute(
            f"SELECT MIN(i_Id) FROM {quote(table)} WHERE i_Area=?", (area,)
        ).fetchone()[0]
        if area == 1:
            replacement = 0.30 if table == "Costume" else 1.20
        else:
            replacement = connection.execute(
                f"SELECT MAX({quote(bonus_column)}) FROM {quote(table)} WHERE i_Area=?",
                (area,),
            ).fetchone()[0]
        cursor = connection.execute(
            f"UPDATE {quote(table)} SET {quote(bonus_column)}=? "
            "WHERE i_Area=? AND i_Id<>?",
            (replacement, area, default_id),
        )
        bonus_changes += cursor.rowcount
        percent = round(float(replacement) * 100 if table == "Costume"
                        else (float(replacement) - 1) * 100)
        for column in sorted(columns(connection, table)):
            if not column.startswith("s_Description_"):
                continue
            descriptions = connection.execute(
                f"SELECT rowid,{quote(column)} FROM {quote(table)} WHERE i_Area=? AND i_Id<>?",
                (area, default_id),
            ).fetchall()
            for rowid, description in descriptions:
                if not isinstance(description, str):
                    continue
                connection.execute(f"UPDATE {quote(table)} SET {quote(column)}=? WHERE rowid=?",
                                   (rewrite_percentage(description, percent), rowid))
    report[f"{table}.nonDefaultBonus"] = bonus_changes


def localized(values: dict[str, str], locale: str) -> str:
    return str(values.get(locale, values.get("en", "")))


def append_memorial_shop_rows(
    connection: sqlite3.Connection, policy: dict[str, Any], report: dict[str, int]
) -> None:
    shop = policy["memorialShop"]
    table_columns = columns(connection, "Shop")
    require_columns(
        connection,
        "Shop",
        {
            "i_Id", "s_ProductID_ios", "s_ProductID_aos", "i_ShopCategory",
            "i_ProductType", "s_ResourceName", "i_RewardGroup", "i_SellType",
            "i_SellValue", "b_IsActive", "i_SortIndex", "s_AreaList",
        },
    )
    locale_columns = {
        "KO": "ko", "EN": "en", "JA": "ja", "ZH_CHS": "zhChs",
        "ZH_CHT": "zhCht", "VI": "vi", "ES": "es", "IT": "it",
        "ID": "id", "TH": "th", "PT": "pt", "HI": "hi",
    }
    inserted = 0
    for item in shop["items"]:
        item_id = int(item["id"])
        if connection.execute("SELECT 1 FROM Shop WHERE i_Id=?", (item_id,)).fetchone():
            raise RuntimeError(f"memorial shop id {item_id} collides with source master")
        values: dict[str, Any] = {
            "i_Id": item_id,
            "i_SortIndex": int(item["sortIndex"]),
            "s_ProductID_ios": item["productIdIos"],
            "s_ProductID_aos": item["productIdAndroid"],
            "i_ShopCategory": int(shop["shopCategory"]),
            "s_AreaList": shop["areaList"],
            "i_ProductType": int(shop["productType"]),
            "s_ResourceName": item["resourceName"],
            "s_AltResourceName": "",
            "i_RewardGroup": int(item["rewardGroup"]),
            "i_Tag": 0,
            "i_SellType": int(shop["sellType"]),
            "i_SellValue": int(shop["sellValue"]),
            "s_Kor_StorePrice_AOS": "$0.00",
            "s_Kor_StorePrice_IOS": "$0.00",
            "s_StorePrice_AOS": "$0.00",
            "s_StorePrice_IOS": "$0.00",
            "b_IsLimitTime": 0,
            "s_UIStartTime": "",
            "s_StartTime": "",
            "s_EndTime": "",
            "b_IsActive": 1,
            "i_Condition": 0,
            "i_ConditionValue": 0,
        }
        for suffix, key in locale_columns.items():
            description = localized(item["description"], key)
            values[f"s_Title_{suffix}"] = localized(item["title"], key)
            values[f"s_Description_{suffix}"] = description
            values[f"s_AltDescription_{suffix}"] = description
        selected = sorted(table_columns)
        connection.execute(
            f"INSERT INTO Shop ({','.join(quote(column) for column in selected)}) "
            f"VALUES ({','.join('?' for _ in selected)})",
            [values.get(column) for column in selected],
        )
        inserted += 1
    report["Shop.memorialRowsAppended"] = inserted


def append_memorial_reward_rows(
    connection: sqlite3.Connection, policy: dict[str, Any], report: dict[str, int]
) -> None:
    require_columns(
        connection,
        "RewardGroup",
        {"i_Id", "i_Group", "i_RewardType", "i_RewardID", "l_RewardQuantity", "i_BuyFirstQuantity"},
    )
    inserted = 0
    for item in policy["memorialShop"]["items"]:
        group = int(item["rewardGroup"])
        if connection.execute("SELECT 1 FROM RewardGroup WHERE i_Group=?", (group,)).fetchone():
            raise RuntimeError(f"memorial reward group {group} collides with source master")
        for index, reward in enumerate(item["rewards"], start=1):
            row_id = group * 100 + index
            if connection.execute("SELECT 1 FROM RewardGroup WHERE i_Id=?", (row_id,)).fetchone():
                raise RuntimeError(f"memorial reward row id {row_id} collides with source master")
            connection.execute(
                "INSERT INTO RewardGroup "
                "(i_Id,i_Group,i_RewardType,i_RewardID,l_RewardQuantity,i_BuyFirstQuantity) "
                "VALUES(?,?,?,?,?,0)",
                (
                    row_id,
                    group,
                    int(reward["type"]),
                    int(reward["id"]),
                    float(reward["quantity"]),
                ),
            )
            inserted += 1
    report["RewardGroup.memorialRowsAppended"] = inserted


def transform_database(asset_name: str, raw: bytes, policy: dict[str, Any]) -> tuple[bytes, dict[str, int]]:
    connection = sqlite3.connect(":memory:")
    connection.deserialize(raw)
    report: dict[str, int] = {}
    multiplier = Decimal(str(policy["requirements"]["multiplier"]))
    tables = [
        str(row[0])
        for row in connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"
        )
    ]
    connection.execute("BEGIN IMMEDIATE")
    try:
        for table in tables:
            table_columns = columns(connection, table)
            if asset_name in COST_CURVE_ASSETS or asset_name.startswith(COST_CURVE_PREFIXES):
                if "d_Cost" in table_columns:
                    scale_curve_costs(connection, table, multiplier, report)
            if table == "PropLevel":
                require_columns(connection, table, {"d_Cost"})
                transform_prop_level_costs(connection, policy, report)
            if table in FORMULA_COST_TABLES:
                transform_formula_costs(connection, table, policy, report)
            if table == "Fan":
                require_columns(connection, table, {"i_Grade", "i_FanCount"})
                rows = connection.execute(
                    "SELECT rowid,i_Area,i_Grade,i_FanCount FROM Fan"
                ).fetchall()
                first_by_area = {
                    area: value
                    for area, value in connection.execute(
                        "SELECT i_Area,i_FanCount FROM Fan WHERE i_Grade=1"
                    )
                }
                changed = 0
                for rowid, area, grade, value in rows:
                    if grade == 1 or value < 0:
                        continue
                    first = first_by_area[area]
                    replacement = first + scaled_decimal(value - first, multiplier)
                    if replacement != value:
                        connection.execute("UPDATE Fan SET i_FanCount=? WHERE rowid=?", (replacement, rowid))
                        changed += 1
                report["Fan.i_FanCount.scaled"] = changed
            if table == "Achievement":
                condition_columns = sorted(
                    column for column in table_columns if column.startswith("s_Condition_")
                )
                if not condition_columns:
                    raise RuntimeError("Achievement has no condition columns")
                scale_columns(connection, table, condition_columns, multiplier, report,
                              tuple(policy["scaledAchievementIds"]))
            if table == "SubscribePass":
                # This is the client's display catalog. The Server's master is
                # extracted from the original bundle, and applies this policy
                # once when handling paidEventPoint. Keep prices/goals intact.
                require_columns(connection, table, {"i_PaidPoint", "i_ADPoint"})
                point_multiplier = policy["passPointMultiplier"]
                if type(point_multiplier) is not int or point_multiplier < 1:
                    raise RuntimeError("passPointMultiplier must be a positive integer")
                for rowid, paid, ad in connection.execute(
                    "SELECT rowid,i_PaidPoint,i_ADPoint FROM SubscribePass"
                ).fetchall():
                    values = (paid, ad)
                    if any(type(value) is not int or value < 0
                           or value * point_multiplier > 2_147_483_647 for value in values):
                        raise RuntimeError("SubscribePass point display outside Int32 range")
                    connection.execute(
                        "UPDATE SubscribePass SET i_PaidPoint=?,i_ADPoint=? WHERE rowid=?",
                        (paid * point_multiplier, ad * point_multiplier, rowid),
                    )
                report["SubscribePass.effectivePointDisplay"] = connection.execute(
                    "SELECT COUNT(*) FROM SubscribePass").fetchone()[0]
            if table == "FollowerProfileLevel":
                require_columns(connection, table, {"d_RequireEXP"})
                # Reward production is already accelerated; do not also lower
                # cumulative thresholds for followers or Lily.
                report["FollowerProfileLevel.thresholdsPreserved"] = connection.execute(
                    "SELECT COUNT(*) FROM FollowerProfileLevel").fetchone()[0]
            if table == "FollowerGiftItem":
                require_columns(connection, table, {"i_Id", "s_ResourceName"})
                expected = {int(key): value for key, value in policy["followerGiftIcons"].items()}
                actual = dict(connection.execute("SELECT i_Id,s_ResourceName FROM FollowerGiftItem"))
                if set(actual) != set(expected) or any(
                    value != "icon_gift_gingerman" for value in actual.values()
                ):
                    raise RuntimeError("FollowerGiftItem icon repair precondition mismatch")
                for gift_id, resource in expected.items():
                    connection.execute(
                        "UPDATE FollowerGiftItem SET s_ResourceName=? WHERE i_Id=?",
                        (resource, gift_id),
                    )
                report["FollowerGiftItem.distinctIcons"] = len(expected)
            requirement_keys = {column.casefold() for column in LEVEL_REQUIREMENT_COLUMNS}
            generic_requirements = sorted(
                column for column in table_columns if column.casefold() in requirement_keys
                and not (table == "Skill" and policy["preserveSkillUnlockRequirements"]
                         and column.casefold() not in {"i_unlockcost", "d_unlockcost"})
            )
            if generic_requirements:
                scale_columns(connection, table, generic_requirements, multiplier, report)
            if table in {"Costume", "Guitar"}:
                transform_cosmetics(
                    connection,
                    table,
                    int(policy["cosmetics"]["purchasablePrice"]),
                    multiplier,
                    report,
                )
            if table == "Shop":
                append_memorial_shop_rows(connection, policy, report)
            if table == "RewardGroup":
                append_memorial_reward_rows(connection, policy, report)
            if table == "Skill":
                require_columns(connection, table, {"i_Id", "f_Cooltime"})
                cooldown_multiplier = float(policy["cooldowns"]["skillMultiplier"])
                connection.execute(
                    "UPDATE Skill SET f_Cooltime=f_Cooltime*? WHERE i_Id NOT IN (4,24)",
                    (cooldown_multiplier,),
                )
                connection.execute(
                    "UPDATE Skill SET f_Cooltime=? WHERE i_Id IN (4,24)",
                    (int(policy["cooldowns"]["skillResetSeconds"]),),
                )
                report["Skill.cooldowns"] = connection.execute("SELECT COUNT(*) FROM Skill").fetchone()[0]
            if table == "MusicLevel":
                require_columns(
                    connection,
                    table,
                    {"f_EncoreBonusAppearCooltime_Sec", "f_EncoreBonusAppearCooltime_Hour"},
                )
                official_base = max(
                    float(row[0])
                    for row in connection.execute(
                        "SELECT f_EncoreBonusAppearCooltime_Sec FROM MusicLevel"
                    )
                    if row[0] is not None
                )
                target_base = float(policy["cooldowns"]["encoreBaseSeconds"])
                connection.execute(
                    "UPDATE MusicLevel SET "
                    "f_EncoreBonusAppearCooltime_Sec=?*(f_EncoreBonusAppearCooltime_Sec/?),"
                    "f_EncoreBonusAppearCooltime_Hour=(?*(f_EncoreBonusAppearCooltime_Sec/?))/3600.0",
                    (target_base, official_base, target_base, official_base),
                )
                report["MusicLevel.encoreCooldowns"] = connection.execute(
                    "SELECT COUNT(*) FROM MusicLevel"
                ).fetchone()[0]
        connection.commit()
        integrity = connection.execute("PRAGMA integrity_check").fetchone()
        if integrity != ("ok",):
            raise RuntimeError(f"transformed {asset_name} integrity check failed: {integrity}")
        return canonicalize_sqlite_header(raw, connection.serialize()), report
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def transform_bundle(
    source: Path,
    output: Path,
    policy_path: Path,
    expected_sha256: str,
    report_path: Path,
) -> None:
    if output.exists() or report_path.exists():
        raise RuntimeError("refusing to overwrite transform output or report")
    source_bytes = source.read_bytes()
    actual = sha256_bytes(source_bytes)
    if actual != expected_sha256.upper():
        raise RuntimeError(f"table bundle SHA-256 {actual} does not match compatibility manifest")
    policy_bytes = policy_path.read_bytes()
    policy = json.loads(policy_bytes)
    if policy.get("version") != 1:
        raise RuntimeError("unsupported memorial policy version")
    try:
        import UnityPy  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("a pinned UnityPy runtime is required") from error

    environment = UnityPy.load(source_bytes)
    transformed_assets: dict[str, dict[str, Any]] = {}
    for obj in environment.objects:
        if obj.type.name != "TextAsset":
            continue
        data = obj.read()
        raw_value = data.m_Script
        raw = (
            raw_value.encode("utf-8", "surrogateescape")
            if isinstance(raw_value, str)
            else bytes(raw_value)
        )
        if not raw.startswith(SQLITE_MAGIC):
            continue
        asset_name = str(data.m_Name)
        transformed, changes = transform_database(asset_name, raw, policy)
        if changes:
            data.m_Script = (
                transformed.decode("utf-8", "surrogateescape")
                if isinstance(raw_value, str)
                else transformed
            )
            data.save()
            transformed_assets[asset_name] = {
                "inputSha256": sha256_bytes(raw),
                "outputSha256": sha256_bytes(transformed),
                "changes": changes,
            }
    required = {
        "Achievement", "CharacterCost", "FollowerCost", "Costume", "Fan", "FollowerProfileLevel",
        "Guitar", "MusicCost", "MusicLevel", "PropLevel", "Skill", "Unit"
    }
    missing = required - transformed_assets.keys()
    if missing:
        raise RuntimeError(f"bundle is missing required transformed assets: {sorted(missing)}")

    output.parent.mkdir(parents=True, exist_ok=True)
    bundle_bytes = environment.file.save(packer="original")
    report = {
        "schema": 1,
        "sourceSha256": actual,
        "outputSha256": sha256_bytes(bundle_bytes),
        "policySha256": sha256_bytes(policy_bytes),
        "assets": transformed_assets,
    }
    for path, payload in ((output, bundle_bytes), (report_path, json.dumps(report, sort_keys=True, indent=2).encode())):
        with tempfile.NamedTemporaryFile(prefix=f".{path.name}.", dir=path.parent, delete=False) as handle:
            temporary = Path(handle.name)
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        try:
            os.link(temporary, path)
        finally:
            temporary.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("policy", type=Path)
    parser.add_argument("--expected-sha256", required=True)
    parser.add_argument("--report", required=True, type=Path)
    arguments = parser.parse_args()
    transform_bundle(
        arguments.source,
        arguments.output,
        arguments.policy,
        arguments.expected_sha256,
        arguments.report,
    )


if __name__ == "__main__":
    main()
