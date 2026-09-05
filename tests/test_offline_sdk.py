"""Portable contract checks; device tests must also verify entry continuation."""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class OfflineSdkTests(unittest.TestCase):
    def test_pending_orders_reply_is_empty_catalog_not_a_receipt(self):
        header = (ROOT / "native-core/include/ggfm/offline_sdk.hpp").read_text(encoding="utf-8")
        payload = re.search(r'R"\((.*?)\)"', header).group(1)
        self.assertEqual(json.loads(payload), {"response_code": 0, "value": []})

    def test_sdk_boundary_is_fingerprinted_without_bypassing_gameplay_state(self):
        manifest = json.loads((ROOT / "compatibility/8.0.0.json").read_text(encoding="utf-8"))
        target = next(x for x in manifest["il2cppHooks"]
                      if x["name"] == "billing.pendingPlatformOrders")
        self.assertEqual(target["rva"], "0x1CE4A70")
        self.assertEqual(target["prologue"], "FE0F1DF8F65701A9F44F02A9152E01B0")
        source = (ROOT / "native-core/src/runtime_hooks.cpp").read_text(encoding="utf-8")
        handler = source.split("void RetryPendingPlatformOrdersHook(", 1)[1].split(
            "void FirebaseManagerInitializeHook", 1)[0]
        self.assertIn('class_get_field(object_get_class(handler), "Success")', handler)
        self.assertIn("field_get_value(handler, field, &listener)", handler)
        self.assertIn("InvokeDelegate(listener, arguments, 1)", handler)
        for forbidden in ("ActivateWaiting", "SetActive", "DoServerPurchase",
                          "eStateType", "purchase_data", "Fail\""):
            self.assertNotIn(forbidden, handler)


if __name__ == "__main__":
    unittest.main()
