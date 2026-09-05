package org.guitargirlresuscitation.memorial;

import android.content.res.AssetManager;

final class MemorialNative {
    static {
        System.loadLibrary("ggfm_bootstrap");
    }

    static native int startServer(
            String dataDir,
            AssetManager assets,
            long unixSeconds,
            int utcOffsetMinutes,
            String capability);
    static void loadGameRuntime() {
        // Use the application's ClassLoader namespace. A native dlopen from the
        // bootstrap worker can create a different linker view from Unity's.
        System.loadLibrary("il2cpp");
    }
    static int prepareRetiredNativeSdks() {
        // startServer installs the loopback-only boundary before this call.
        // Loading the original library does not create a Firebase App; it only
        // exposes the pinned Messaging entry points for deterministic Hooks.
        System.loadLibrary("FirebaseCppApp-13_4_0");
        return installRetiredNativeSdkHooks();
    }
    private static native int installRetiredNativeSdkHooks();
    static native int installRuntimeHooks(boolean firebaseOnlyDiagnostic);
    static native int armRuntimeHooks(boolean firebaseOnlyDiagnostic);
    static native String endpoint();
    static native String drainServerLogs();
    static native String startupBanner(int columns);
    static native int transferSave(String dataDir, String stagingPath, boolean importing);
    static native int prepareShutdown();
    static native int stop();
    static native boolean queueLegacySelection(long generation, int index);
    static native boolean queueCurrencyAmount(long generation, String amount);

    private MemorialNative() {}
}
