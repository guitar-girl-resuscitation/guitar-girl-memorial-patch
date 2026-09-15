import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]

class FanCacheTests(unittest.TestCase):
    def test_complete_boundary_on_both_abis_without_ui_throttling(self):
        expected = {'gameplay.fan.multiplier', 'gameplay.fan.clear',
                    'gameplay.fan.setRow', 'gameplay.fan.loadRpc',
                    'gameplay.fan.loadBytes', 'ads.requestAndLoad'}
        for filename in ('8.0.0.json', '8.0.0-armv7.json'):
            manifest = json.loads((ROOT/'compatibility'/filename).read_text())
            hooks = {x['name'] for x in manifest['il2cppHooks']}
            self.assertTrue(expected <= hooks, filename)
            self.assertFalse(any(x.startswith('gameplay.followerRow') for x in hooks))
            self.assertIn('gameplay.fan.level', {x['name'] for x in manifest['il2cppDependencies']})

    def test_real_native_cache_wrappers(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('host compiler unavailable; CI runs this test')
        with tempfile.TemporaryDirectory(prefix='ggfm-fan-cache-') as temp:
            exe = Path(temp)/'cache-test.exe'
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT/'tests/fixtures/fan-cache'),
                            '-I', str(ROOT/'native-core/include'),
                            str(ROOT/'native-core/tests/fan_multiplier_cache_test.cpp'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
