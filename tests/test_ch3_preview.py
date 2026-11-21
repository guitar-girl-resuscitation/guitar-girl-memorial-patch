import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class Ch3PreviewTests(unittest.TestCase):
    def test_selected_hint_slots(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("host C++ compiler unavailable")
        with tempfile.TemporaryDirectory() as temp:
            exe = Path(temp) / "preview-test.exe"
            subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                            "-I", str(ROOT / "native-core/include"),
                            str(ROOT / "native-core/tests/ch3_preview_test.cpp"),
                            "-o", str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
