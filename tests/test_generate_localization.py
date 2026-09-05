import json
import tempfile
import unittest
from pathlib import Path

from tools.generate_localization import KEYS, generate


ROOT = Path(__file__).parents[1]


class LocalizationGeneratorTests(unittest.TestCase):
    def test_all_client_locales_generate_without_raw_unicode(self):
        output = generate(ROOT / "localization" / "strings.json")
        self.assertIn("kGeneratedLocales", output)
        self.assertNotIn("纪念版功能", output)
        self.assertEqual(output.count("LocaleText{"), 12)

    def test_incomplete_locale_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "strings.json"
            path.write_text(json.dumps({"fallback": "en", "locales": {"en": {}}}))
            with self.assertRaises(ValueError):
                generate(path)

    def test_key_contract_is_explicit(self):
        self.assertIn("restart", KEYS)
        self.assertIn("owned", KEYS)


if __name__ == "__main__":
    unittest.main()
