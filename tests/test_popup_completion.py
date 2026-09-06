import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class PopupCompletionTests(unittest.TestCase):
    def test_both_architectures_install_the_pair(self):
        for name in ('8.0.0.json', '8.0.0-armv7.json'):
            data = json.loads((ROOT/'compatibility'/name).read_text(encoding='utf-8'))
            hooks = {row['name'] for row in data['il2cppHooks']}
            self.assertTrue({'ui.popupEndScale', 'ui.popupWaitOpen'} <= hooks)
            self.assertNotIn('ui.popupOpenInstant', hooks)
            self.assertTrue({'ui.shop.confirm', 'ui.shopDetail.confirm',
                             'ui.infoFanCostume.confirm', 'ui.unlockFanCostume.confirm'} <= hooks)
            self.assertIn('ui.popupBase.confirm', {row['name'] for row in data['il2cppDependencies']})
            self.assertNotIn('ui.popupOpenAnimation', hooks)
            self.assertTrue({'popup.openCallback', 'popup.closing', 'popup.waitState'} <= data['il2cppFields'].keys())

    def test_stock_opening_animation_is_preserved(self):
        source = (ROOT/'native-core/src/runtime_hooks.cpp').read_text(encoding='utf-8')
        self.assertNotIn('void PopupOpenHook(', source)
        self.assertNotIn('popup_stop_tweens_address', source)

    def test_native_completion_lifecycle(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('host C++ compiler unavailable; CI runs native lifecycle test')
        with tempfile.TemporaryDirectory(prefix='ggfm-popup-') as temp:
            exe = Path(temp)/'popup-test.exe'
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(ROOT/'native-core/include'),
                            str(ROOT/'native-core/tests/popup_completion_test.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
