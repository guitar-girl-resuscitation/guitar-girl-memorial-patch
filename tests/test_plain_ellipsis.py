import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).parents[1]


class PlainEllipsisTests(unittest.TestCase):
    def test_both_abis_have_complete_boundary(self):
        for name in ('8.0.0.json', '8.0.0-armv7.json'):
            manifest = json.loads((ROOT / 'compatibility' / name).read_text())
            hooks = {entry['name'] for entry in manifest['il2cppHooks']}
            self.assertTrue({'ui.label.processText', 'ui.label.wrapText'} <= hooks)
            self.assertIn('label.encoding', manifest['il2cppFields'])
            self.assertIn('label.processedText', manifest['il2cppFields'])

    def test_real_wrappers_before_measurement(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('host compiler unavailable; CI runs this test')
        source = (ROOT / 'native-core/src/runtime_hooks.cpp').read_text()
        # Compile the production wrappers with mocked managed allocation/wrapping.
        wrappers = source.split('using ProcessLabel =', 1)[1].split(
            'UrlGetter unused_url_original', 1)[0]
        with tempfile.TemporaryDirectory(prefix='ggfm-ellipsis-') as temp:
            directory = Path(temp)
            (directory / 'runtime_wrappers.inc').write_text(
                'using ProcessLabel =' + wrappers)
            exe = directory / 'ellipsis-test.exe'
            subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                            '-I', str(directory),
                            '-I', str(ROOT / 'native-core/include'),
                            str(ROOT / 'native-core/tests/plain_ellipsis_test.cpp'),
                            '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
