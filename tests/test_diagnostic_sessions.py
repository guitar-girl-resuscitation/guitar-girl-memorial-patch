from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class DiagnosticHistoryTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("javac") and shutil.which("java"), "requires JDK")
    def test_process_kill_and_repeated_gate_export_keep_last_five_runs(self):
        package = "org.guitargirlresuscitation.memorial"
        source = Path(__file__).resolve().parents[1] / "bootstrap/src/main/java" / package.replace(".", "/")
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture = root / "Run.java"
            fixture.write_text("""
package org.guitargirlresuscitation.memorial;
import java.io.File;
class Run {
    public static void main(String[] args) throws Exception {
        DiagnosticSessions journal = new DiagnosticSessions(new File(args[0]));
        if (args.length == 1) {
            System.out.print(journal.history(""));
            return;
        }
        journal.start("boot-" + args[1], Long.parseLong(args[1]));
        journal.event("music.unlock id=" + args[1]);
        System.out.println("written");
        System.out.flush();
        Thread.sleep(60000);
    }
}
""", encoding="utf-8")
            subprocess.run(["javac", "--release", "8", "-d", tmp,
                            str(source / "DiagnosticSessions.java"), str(fixture)], check=True)
            command = ["java", "-cp", tmp, package + ".Run", str(root / "sessions")]
            for i in range(1, 8):
                child = subprocess.Popen([*command, str(i)], stdout=subprocess.PIPE, text=True)
                try:
                    self.assertEqual(child.stdout.readline().strip(), "written")
                finally:
                    child.kill()
                    child.wait(timeout=10)
                    child.stdout.close()
            history = subprocess.check_output(command, text=True)
            self.assertEqual(history, subprocess.check_output(command, text=True))
            self.assertEqual(history.count("=== Session "), 5)
            self.assertEqual([line for line in history.splitlines() if line.startswith("boot-")],
                             ["boot-7", "boot-6", "boot-5", "boot-4", "boot-3"])
            for i in range(3, 8):
                self.assertIn("music.unlock id=" + str(i), history)
