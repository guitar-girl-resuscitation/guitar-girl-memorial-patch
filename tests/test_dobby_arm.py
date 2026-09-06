import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("fix_dobby_arm", Path(__file__).resolve().parents[1] / "tools/fix_dobby_arm.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DobbyArmTests(unittest.TestCase):
    def test_checked_transform_preserves_thumb_and_dry_run(self):
        source = "prefix\n" + module.START + "old ARM branch block\n" + module.END + "tail\n// relocate thumb-1 instructions\n" + module.END
        expected = hashlib.sha256(source.encode()).hexdigest()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / module.SOURCE
            path.parent.mkdir(parents=True)
            path.write_text(source, encoding="utf-8", newline="\n")
            with patch.object(module, "EXPECTED", expected):
                module.fix(root)
                self.assertEqual(path.read_text(), source)
                module.fix(root, apply=True)
                self.assertEqual(path.with_suffix(path.suffix + ".ggfm-original").read_text(), source)
                transformed = path.read_text()
                self.assertTrue(transformed.endswith("// relocate thumb-1 instructions\n" + module.END))
                self.assertIn("word & 0xff000000u", transformed)
                self.assertIn("<< 8) >> 6", transformed)
                with self.assertRaises(ValueError):
                    module.fix(root, apply=True)

    def test_unknown_source_is_rejected_without_writes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / module.SOURCE
            path.parent.mkdir(parents=True)
            path.write_bytes(b"unknown")
            with self.assertRaises(ValueError):
                module.fix(root, apply=True)
            self.assertEqual(path.read_bytes(), b"unknown")
            self.assertFalse(path.with_suffix(path.suffix + ".ggfm-original").exists())


class ArmBranchExecutionTests(unittest.TestCase):
    """Independent instruction-level model of the replacement branch layout.

    Full compiled Dobby integration is additionally tested in the device game.
    """
    def test_taken_not_taken_forward_backward_and_thumb_calls(self):
        try:
            import unicorn as uc
            from unicorn import arm_const as reg
        except ImportError:
            self.skipTest("optional Unicorn instruction execution check")
        import struct
        # The LDR stub is at +8. Return/fallthrough at +4 skips its literal at +12.
        for link, condition, taken, target in [
                (True, 14, True, 0x1800), (True, 14, True, 0x2800),
                (False, 14, True, 0x1800), (True, 0, False, 0x1800),
                (True, 14, True, 0x1801)]:
            with self.subTest(link=link, condition=condition, target=target, taken=taken):
                emu = uc.Uc(uc.UC_ARCH_ARM, uc.UC_MODE_ARM)
                emu.mem_map(0x1000, 0x3000)
                word = (condition << 28) | (0x0b000000 if link else 0x0a000000)
                emu.mem_write(0x2000, struct.pack("<IIII", word, 0xea000001, 0xe51ff004, target))
                emu.mem_write(target & ~1, b"\x01\x30\x70\x47" if target & 1 else struct.pack("<II", 0xe2800001, 0xe12fff1e))
                emu.reg_write(reg.UC_ARM_REG_CPSR, 0x10)  # Z=0: EQ is false.
                emu.reg_write(reg.UC_ARM_REG_LR, 0x2010)
                emu.emu_start(0x2000, 0x2010, count=20)
                self.assertEqual(emu.reg_read(reg.UC_ARM_REG_PC), 0x2010)
                self.assertEqual(emu.reg_read(reg.UC_ARM_REG_R0), int(taken))
