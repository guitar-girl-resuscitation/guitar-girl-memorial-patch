import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("harden_dobby", Path(__file__).parents[1] / "tools/harden_dobby.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class AllocatorPatchTests(unittest.TestCase):
    def fixture(self, root):
        expected = {}
        for name in (module.POSIX, module.ARENA):
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture\r\n")
            expected[name] = hashlib.sha256(b"fixture\n").hexdigest()
        return expected

    def test_dry_run_and_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = self.fixture(root)
            with patch.object(module, "EXPECTED", expected), patch.object(module, "transform", return_value="safe\n"):
                module.harden(root)
                self.assertEqual((root / module.POSIX).read_bytes(), b"fixture\r\n")
                module.harden(root, True)
                self.assertEqual((root / module.POSIX).read_bytes(), b"safe\n")
                self.assertEqual((root / module.POSIX).with_suffix(".cc.ggfm-original").read_bytes(), b"fixture\r\n")
                with self.assertRaises(ValueError):
                    module.harden(root, True)

    def test_mismatched_second_file_never_partially_patches(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            expected = self.fixture(root)
            (root / module.ARENA).write_bytes(b"unknown dependency")
            with patch.object(module, "EXPECTED", expected), patch.object(module, "transform", return_value="safe\n"):
                with self.assertRaises(ValueError):
                    module.harden(root, True)
            self.assertEqual((root / module.POSIX).read_bytes(), b"fixture\r\n")
            self.assertFalse((root / module.POSIX).with_suffix(".cc.ggfm-original").exists())


if __name__ == "__main__":
    unittest.main()
