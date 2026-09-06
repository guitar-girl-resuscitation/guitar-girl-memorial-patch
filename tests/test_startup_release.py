"""Resource-free startup presentation regression checks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "bootstrap/src/main/java/org/guitargirlresuscitation/memorial"


class StartupReleaseTests(unittest.TestCase):
    def test_failure_drains_native_logs_and_keeps_export_available(self):
        startup = (JAVA / "MemorialStartupActivity.java").read_text(encoding="utf-8")
        error = startup.split('private void appendError(String line)', 1)[1].split('private void pauseMilestone', 1)[0]
        self.assertLess(error.index('drainServerLogs()'), error.index('logPumpRunning = false'))
        self.assertIn('Intent.ACTION_CREATE_DOCUMENT', startup)
        self.assertIn('exportDiagnostics(true)', startup)
        self.assertIn('diagnostics.native-drain', startup)
        self.assertIn('stage=" + startupStage', startup)

    def test_update_prompt_is_optional_and_uses_only_verified_availability(self):
        source = (JAVA / "UpdateController.java").read_text(encoding="utf-8")
        self.assertIn('if (!available || promptShown) return;', source)
        self.assertIn('.setNegativeButton(DiagnosticText.get(6), null)', source)
        self.assertIn('-> openWebsite()', source)
        self.assertIn('UpdateRules.compatible', source)
        self.assertIn('UpdateRules.newer', source)
        self.assertNotIn('elapsed < 24', source)
        self.assertIn('setInstanceFollowRedirects(false)', source)

    def test_copy_uses_visible_diagnostics_and_update_failure_is_only_informational(self):
        startup = (JAVA / "MemorialStartupActivity.java").read_text(encoding="utf-8")
        self.assertIn('view -> copyDiagnostics()', startup)
        self.assertIn('newPlainText("GGFM diagnostics", visibleLog.toString())', startup)
        self.assertIn('logView.setTextIsSelectable(true)', startup)
        updates = (JAVA / "UpdateController.java").read_text(encoding="utf-8")
        self.assertIn('diagnostic.accept("[INFO] update.check: " + failure.getClass()', updates)

    def test_java_fatal_journal_preserves_platform_handler(self):
        source = (JAVA / "DiagnosticJournal.java").read_text(encoding="utf-8")
        self.assertIn('previous.uncaughtException(thread, failure)', source)
        self.assertIn('.putString("fatal_java", report).commit()', source)
        self.assertIn('report.length() > 12000', source)

    def test_banner_linker_failure_retains_actionable_diagnostics(self):
        startup = (JAVA / "MemorialStartupActivity.java").read_text(encoding="utf-8")
        self.assertIn('startupFailureDetails(failure)', startup)
        self.assertIn('failure.getMessage()', startup)
        self.assertIn('failure.getCause()', startup)
        self.assertIn('Build.SUPPORTED_ABIS', startup)
        self.assertIn('getApplicationInfo().splitSourceDirs', startup)
        self.assertNotIn('appendError("bootstrap: " + failure.getClass().getSimpleName())', startup)

    def test_guitar_is_tuned_only_by_the_final_ritual(self):
        startup = (JAVA / "MemorialStartupActivity.java").read_text(encoding="utf-8")
        terminal = (JAVA / "TerminalText.java").read_text(encoding="utf-8")
        self.assertNotIn("SIX STRINGS", startup)
        self.assertIn("AIRISUTEK PATCH LINK", startup)
        self.assertEqual(terminal.count("> tune guitar"), 1)


if __name__ == "__main__":
    unittest.main()
