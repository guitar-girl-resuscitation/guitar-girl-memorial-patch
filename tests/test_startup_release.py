"""Resource-free startup presentation regression checks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "bootstrap/src/main/java/org/guitargirlresuscitation/memorial"


class StartupReleaseTests(unittest.TestCase):
    def test_guitar_is_tuned_only_by_the_final_ritual(self):
        startup = (JAVA / "MemorialStartupActivity.java").read_text(encoding="utf-8")
        terminal = (JAVA / "TerminalText.java").read_text(encoding="utf-8")
        self.assertNotIn("SIX STRINGS", startup)
        self.assertIn("AIRISUTEK PATCH LINK", startup)
        self.assertEqual(terminal.count("> tune guitar"), 1)


if __name__ == "__main__":
    unittest.main()
