import unittest
from unittest.mock import patch

from tools.transform_bundle_metadata import rewrite_manifest, rewrite_size_text


class BundleMetadataTests(unittest.TestCase):
    def test_sizes_change_only_named_android_bundle_rows(self):
        text = "other.ab 91\ntable/table_db.ab 100\ntable/table_bundlesize.ab 20\n"
        result = rewrite_size_text(text, 123456, 987)
        self.assertEqual(result, "other.ab 91\ntable/table_db.ab 123456\ntable/table_bundlesize.ab 987\n")

    def test_missing_or_duplicate_size_target_fails_closed(self):
        for text in ("other.ab 1\n", "table/table_db.ab 1\ntable/table_db.ab 2\n"):
            with self.assertRaises(ValueError):
                rewrite_size_text(text, 3, 4)

    @patch("tools.transform_bundle_metadata.content_identity")
    def test_crc_and_cache_identifier_both_change(self, identity):
        identity.side_effect = [(42, bytes(16)), (99, bytes(range(16)))]
        source = (b"CRC: 42\nHashes:\n  AssetFileHash:\n"
                  b"    serializedVersion: 2\n    Hash: abcdef\n"
                  b"  TypeTreeHash:\n    serializedVersion: 2\n    Hash: fedcba\n")
        result = rewrite_manifest(source, b"before", b"after")
        self.assertIn(b"CRC: 99\n", result)
        self.assertIn(b"Hash: 000102030405060708090a0b0c0d0e0f\n", result)
        self.assertIn(b"Hash: fedcba\n", result)

    @patch("tools.transform_bundle_metadata.content_identity", return_value=(42, bytes(16)))
    def test_invalid_source_crc_fails_closed(self, _identity):
        with self.assertRaises(ValueError):
            rewrite_manifest(b"CRC: 43\n", b"before", b"after")


if __name__ == "__main__":
    unittest.main()
