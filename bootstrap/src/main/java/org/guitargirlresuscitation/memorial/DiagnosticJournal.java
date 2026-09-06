package org.guitargirlresuscitation.memorial;

import android.content.Context;
import android.util.Log;
import java.util.concurrent.atomic.AtomicBoolean;

/** Java failures only: preserve the platform crash handler; never swallow a crash. */
final class DiagnosticJournal {
    private static final AtomicBoolean INSTALLED = new AtomicBoolean();
    static void install(Context context) {
        if (!INSTALLED.compareAndSet(false, true)) return;
        Context app = context.getApplicationContext();
        Thread.UncaughtExceptionHandler previous = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, failure) -> {
            try {
                String report = "[ERROR] uncaught Java failure; thread=" + thread.getName()
                        + "\n" + Log.getStackTraceString(failure);
                if (report.length() > 12000) report = report.substring(0, 12000) + "\n[truncated]";
                // Synchronous only on fatal error, before the platform kills us.
                app.getSharedPreferences("ggfm_diagnostics", Context.MODE_PRIVATE).edit()
                        .putString("fatal_java", report).commit();
            } catch (Throwable ignored) {
                // Diagnostics must not replace the original crash or handler.
            } finally {
                if (previous != null) previous.uncaughtException(thread, failure);
                else {
                    android.os.Process.killProcess(android.os.Process.myPid());
                    System.exit(10);
                }
            }
        });
    }
    private DiagnosticJournal() {}
}
