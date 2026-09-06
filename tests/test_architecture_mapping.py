import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("mapping", Path(__file__).parents[1] / "tools/audit_architecture_mapping.py")
mapping = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mapping)


class MappingTests(unittest.TestCase):
    def run_audit(self, candidates):
        method = {"Address": 16, "Name": "Demo$$Run", "Signature": "void Run(MethodInfo_ABCD* info)", "TypeSignature": "vi"}
        return mapping.audit({"il2cppHooks": [{"name": "example", "rva": "0x10"}], "il2cppDependencies": []},
                             {"ScriptMethod": [method]}, {"ScriptMethod": candidates})

    def test_address_suffix_is_not_identity(self):
        result = self.run_audit([{"Address": 33, "Name": "Demo$$Run", "Signature": "void Run(MethodInfo_1234* info)", "TypeSignature": "vi"}])
        self.assertEqual(result["matched"], 1)
        self.assertEqual(result["methods"][0]["destinationRva"], "0x21")
        self.assertFalse(result["releaseReady"])

    def test_same_name_different_signature_is_rejected(self):
        result = self.run_audit([{"Address": 33, "Name": "Demo$$Run", "Signature": "int Run(MethodInfo* info)", "TypeSignature": "ii"}])
        self.assertEqual(result["matched"], 0)

    def test_ambiguous_identity_is_rejected(self):
        method = {"Address": 33, "Name": "Demo$$Run", "Signature": "void Run(MethodInfo* info)", "TypeSignature": "vi"}
        self.assertEqual(self.run_audit([method, dict(method, Address=44)])["matched"], 0)


if __name__ == "__main__":
    unittest.main()
