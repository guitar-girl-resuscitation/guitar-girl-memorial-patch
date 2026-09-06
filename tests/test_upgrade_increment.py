import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class UpgradeIncrementTests(unittest.TestCase):
    def test_both_abis_repair_rpc_constructors(self):
        for filename in ('8.0.0.json', '8.0.0-armv7.json'):
            data = json.loads((ROOT/'compatibility'/filename).read_text(encoding='utf-8'))
            hooks = {item['name'] for item in data['il2cppHooks']}
            self.assertTrue({'gameplay.upgrade.skillConstructor',
                             'gameplay.upgrade.unitConstructor'} <= hooks)
            self.assertIn('gameplay.upgrade.encodeFloat',
                          {item['name'] for item in data['il2cppDependencies']})
            for name in ('row', 'skillDto', 'unitDto'):
                self.assertGreater(int(data['il2cppFields'][f'upgrade.{name}.increment'], 16), 0)

    def test_native_fractional_projection(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('host C++ compiler unavailable; CI runs native test')
        with tempfile.TemporaryDirectory(prefix='ggfm-upgrade-') as temp:
            exe = Path(temp)/'upgrade-test.exe'
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT/'native-core/include'),
                            str(ROOT/'native-core/tests/upgrade_increment_test.cpp'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
