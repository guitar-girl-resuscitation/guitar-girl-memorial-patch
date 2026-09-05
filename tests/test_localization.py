import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class LocalizationTests(unittest.TestCase):
    def test_all_client_languages_have_complete_memorial_text(self):
        document = json.loads((ROOT / "localization" / "strings.json").read_text(encoding="utf-8"))
        expected_locales = {
            "ko", "en", "ja", "zh-Hans", "zh-Hant", "vi",
            "es", "it", "id", "th", "pt", "hi",
        }
        self.assertEqual(expected_locales, set(document["locales"]))
        keys = set(document["locales"][document["fallback"]])
        self.assertTrue(keys)
        for locale, messages in document["locales"].items():
            self.assertEqual(keys, set(messages), locale)
            self.assertTrue(all(value.strip() for value in messages.values()), locale)
