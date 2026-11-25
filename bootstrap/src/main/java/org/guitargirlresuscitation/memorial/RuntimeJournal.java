package org.guitargirlresuscitation.memorial;

import android.content.Context;
import android.os.Process;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.regex.*;

/** Bounded, app-private operation history. Keep only structured events or error identifiers. */
final class RuntimeJournal {
    private static final Pattern UNITY_ERROR = Pattern.compile("\\b(?:[A-Za-z0-9_.]+Exception|90007|[0-9]{5,6}(?=\\b))\\b");
    private static DiagnosticSessions sessions;
    private static boolean running;

    static synchronized String history(Context context, String legacy) {
        if (sessions == null)
            sessions = new DiagnosticSessions(new File(context.getFilesDir(), "diagnostic-sessions"));
        try { return sessions.history(legacy); }
        catch (IOException error) { return legacy + "\njournal read failed: " + error.getClass().getSimpleName(); }
    }

    static synchronized void start(String initial) {
        if (sessions == null || running) return;
        try { sessions.start(initial, System.currentTimeMillis()); }
        catch (IOException error) { android.util.Log.w("GGFM", "journal start failed", error); }
        running = true;
        Thread reader = new Thread(RuntimeJournal::collect, "ggfm-diagnostics");
        reader.setDaemon(true);
        reader.start();
    }

    static synchronized String current() {
        if (sessions == null) return "";
        try { return sessions.current(); }
        catch (IOException error) { return "journal read failed: " + error.getClass().getSimpleName(); }
    }

    static synchronized void startupLine(String message) {
        if (!running) return;
        try { sessions.startupLine(message); }
        catch (IOException error) { android.util.Log.w("GGFM", "journal write failed", error); }
    }

    static synchronized void event(String message) {
        if (!running) return;
        try { sessions.event(message); }
        catch (IOException error) { android.util.Log.w("GGFM", "journal write failed", error); }
    }

    private static void collect() {
        java.lang.Process child = null;
        try {
            child = new ProcessBuilder("logcat", "-v", "tag", "--pid=" + Process.myPid(), "-T", "1",
                    "GGFM:I", "GGFM-SERVER:I", "Unity:V", "*:S").start();
            try (BufferedReader lines = new BufferedReader(new InputStreamReader(child.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = lines.readLine()) != null) {
                    int separator = line.indexOf(':');
                    if (separator < 0) continue;
                    String tag = line.substring(0, separator).trim();
                    String msg = line.substring(separator + 1).trim();
                    if (tag.endsWith("/GGFM-SERVER") && msg.matches(
                            "request dispatch: category=(?:user|store) call=(?:buyMusic|buyShop|buyPackage|userLoad)"))
                        event(msg);
                    else if ((tag.endsWith("/GGFM") || tag.endsWith("/GGFM-SERVER")) && msg.startsWith("diag "))
                        event(msg);
                    else if (tag.endsWith("/Unity") && (tag.startsWith("E/") || msg.contains("Error") || msg.contains("90007"))) {
                        Matcher error = UNITY_ERROR.matcher(msg);
                        if (error.find()) event("unity error=" + error.group());
                    }
                }
            }
            event("collector stopped");
        } catch (IOException | SecurityException e) { event("collector unavailable=" + e.getClass().getSimpleName()); }
        finally { if (child != null) child.destroy(); }
    }
}
