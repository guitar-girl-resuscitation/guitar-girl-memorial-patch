import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_hook_targets", ROOT / "tools" / "generate_hook_targets.py"
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class HookGeneratorTests(unittest.TestCase):
    def test_playerprefs_save_is_native_but_key_isolation_remains(self):
        manifest = json.loads((ROOT / "compatibility/8.0.0.json").read_text(encoding="utf-8"))
        names = {row["name"] for row in manifest["il2cppHooks"]}
        self.assertNotIn("prefs.save", names)
        self.assertTrue({"prefs.setInt", "prefs.getInt", "prefs.setString",
                         "prefs.getString", "prefs.deleteKey", "prefs.deleteAll"} <= names)

    def test_settings_disable_uses_body_not_adjacent_language_stub(self):
        manifest = json.loads((ROOT / "compatibility/8.0.0.json").read_text(encoding="utf-8"))
        entry = next(row for row in manifest["il2cppHooks"] if row["name"] == "ui.setting.disable")
        self.assertEqual(entry["rva"], "0x1E1C368")
        self.assertNotEqual(entry["rva"], "0x1E1CB60")
        loader = (ROOT / "native-core/src/standalone_loader.cpp").read_text(encoding="utf-8")
        self.assertIn("dobby_enable_near_branch_trampoline();", loader)
        core = (ROOT / "native-core/src/core.cpp").read_text(encoding="utf-8")
        self.assertIn("patch_span = sizeof(void*) == 4 ? 8 : 4", core)
        self.assertIn("std::memcmp(before + patch_span, after + patch_span", core)

    def test_pass_selection_is_a_scrollable_list_with_generation_checked_events(self):
        source = (ROOT / "native-core/src/memorial_ui.cpp").read_text(encoding="utf-8")
        page = source.split("void ShowPassList()", 1)[1].split("void ShowSaveList", 1)[0]
        self.assertIn("ShowMemorialList", page)
        self.assertNotIn("Action::Previous", page)
        self.assertNotIn("Action::Next", page)
        self.assertIn("menu.page == Page::PassList && selected->index < menu.seasons.size()", source)

    def test_client_upgrade_save_uses_checked_stock_entrypoints(self):
        manifest = json.loads((ROOT / "compatibility" / "8.0.0.json").read_text(encoding="utf-8"))
        entries = {entry["name"]: entry for entry in
                   manifest["il2cppHooks"] + manifest["il2cppDependencies"]}
        expected = {
            "gameplay.save.characterUpgrade": "0x1CB3890",
            "gameplay.save.followerUpgrade": "0x1CACA4C",
            "gameplay.save.manager": "0x21A6B94",
            "gameplay.save.upload": "0x2061654",
        }
        for name, rva in expected.items():
            self.assertEqual(entries[name]["rva"], rva)
            self.assertEqual(len(bytes.fromhex(entries[name]["prologue"])), 16)

    def test_checked_manifest_generates_every_entry(self):
        manifest_path = ROOT / "compatibility" / "8.0.0.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        generated = MODULE.generate(manifest_path)
        for entry in manifest["il2cppHooks"] + manifest["il2cppDependencies"]:
            self.assertIn(f'"{entry["name"]}"', generated)
        for name in manifest["il2cppFields"]:
            self.assertIn(f'"{name}"', generated)

    def test_ch3_buy_ap_guard_preserves_tested_lsposed_boundary(self):
        manifest = json.loads((ROOT / "compatibility" / "8.0.0.json").read_text(encoding="utf-8"))
        target = next(item for item in manifest["il2cppHooks"] if item["name"] == "gameplay.ch3.buyApGuard")
        self.assertEqual(target["rva"], "0x22A3F6C")
        self.assertEqual(target["prologue"], "FE0F1FF8084440B91F09007121010054")
        source = (ROOT / "native-core" / "src" / "gameplay_compat.cpp").read_text(encoding="utf-8")
        self.assertIn("RetiredBuyApClick", source)
        self.assertIn('name == "gameplay.ch3.buyApGuard"', source)

    def test_settings_entry_survives_stock_auth_visibility_refresh(self):
        manifest_path = ROOT / "compatibility" / "8.0.0.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        targets = {entry["name"]: entry for entry in manifest["il2cppHooks"]}
        self.assertEqual(targets["ui.setting.show"]["rva"], "0x1E1C574")
        self.assertEqual(
            targets["ui.setting.authVisibility"]["rva"], "0x1E1B6CC"
        )
        self.assertIn("setting.row.auth.offlineParent", manifest["il2cppFields"])
        self.assertIn("setting.row.auth.offline", manifest["il2cppFields"])

    def test_memorial_notice_keeps_stock_popup_without_shutdown(self):
        manifest_path = ROOT / "compatibility" / "8.0.0.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        targets = {entry["name"]: entry for entry in manifest["il2cppHooks"]}
        dependencies = {
            entry["name"]: entry for entry in manifest["il2cppDependencies"]
        }
        self.assertEqual(
            {
                "notice.maintenanceHandlerObject",
                "notice.maintenanceHandlerShared",
                "notice.maintenancePopup",
                "notice.goToInGame",
                "notice.goToEventIntro",
            },
            set(targets).intersection(
                {
                    "notice.maintenanceHandlerObject",
                    "notice.maintenanceHandlerShared",
                    "notice.maintenancePopup",
                    "notice.goToInGame",
                    "notice.goToEventIntro",
                }
            ),
        )
        self.assertEqual(
            dependencies["notice.requestUpdateTime"]["rva"], "0x1D05B88"
        )
        source = (ROOT / "native-core" / "src" / "runtime_hooks.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("OpenMemorialNoticeWithoutShutdown", source)
        self.assertIn("on_ok = nullptr", source)
        self.assertIn("on_cancel = nullptr", source)
        self.assertIn("duplicate memorial notice suppressed", source)
        self.assertIn("memorial_notice_seen_this_process.exchange", source)

    def test_rejects_non_16_byte_fingerprint(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad.json"
            path.write_text(
                json.dumps({"il2cppHooks": [{"name": "bad", "rva": "0x1", "prologue": "00"}]}),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                MODULE.generate(path)

    def test_rejects_duplicate_names(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = Path(temp_dir) / "bad.json"
            item = {"name": "same", "rva": "0x1", "prologue": "00" * 16}
            path.write_text(
                json.dumps({"il2cppHooks": [item], "il2cppDependencies": [item]}),
                encoding="utf-8",
            )
            with self.assertRaises(ValueError):
                MODULE.generate(path)
