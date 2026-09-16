from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]


class LargeNumberDisplayTests(unittest.TestCase):
    def test_bounded_native_display(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('host compiler unavailable; CI runs this test')
        with tempfile.TemporaryDirectory(prefix='ggfm-big-display-') as temp:
            exe = Path(temp) / 'display-test.exe'
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT / 'native-core/include'),
                            str(ROOT / 'native-core/tests/large_number_display_test.cpp'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
