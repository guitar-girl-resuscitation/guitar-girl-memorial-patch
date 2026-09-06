package org.guitargirlresuscitation.memorial;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import org.json.JSONObject;
import javax.net.ssl.HttpsURLConnection;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/** Advisory updates only. No APK downloads, installation, identifiers or save uploads. */
final class UpdateController {
    interface Listener { void result(String message, boolean available); }
    private final Activity activity;
    private final Listener listener;
    private final java.util.function.Consumer<String> diagnostic;
    private boolean promptShown;
    private boolean promptVisible;
    private final AtomicBoolean busy = new AtomicBoolean();
    private String origin;
    private String signer;
    private long installedCode;
    private String installedName = "";

    UpdateController(Activity activity, Listener listener, java.util.function.Consumer<String> diagnostic) {
        this.activity = activity;
        this.listener = listener;
        this.diagnostic = diagnostic;
        try {
            PackageInfo info = activity.getPackageManager().getPackageInfo(activity.getPackageName(), PackageManager.GET_SIGNING_CERTIFICATES);
            installedCode = info.getLongVersionCode();
            installedName = info.versionName;
            android.content.pm.Signature[] signatures = info.signingInfo.getApkContentsSigners();
            if (signatures.length != 1) return;
            StringBuilder hash = new StringBuilder();
            for (byte b : MessageDigest.getInstance("SHA-256").digest(signatures[0].toByteArray()))
                hash.append(String.format(Locale.ROOT, "%02X", b & 255));
            signer = hash.toString();
            JSONObject source;
            try (InputStream stream = activity.getAssets().open("ggfm/update-source.json")) {
                source = readJson(stream);
            }
            if (source.getInt("schema") != 1 || !activity.getPackageName().equals(source.getString("applicationId"))
                    || !signer.equalsIgnoreCase(source.getString("signerSha256"))
                    || source.getLong("versionCode") != installedCode) return;
            String candidate = source.optString("origin", "");
            URL url = new URL(candidate);
            if (!"https".equals(url.getProtocol()) || url.getHost().isEmpty() || url.getUserInfo() != null
                    || !url.getPath().isEmpty() || url.getQuery() != null || url.getRef() != null) return;
            origin = candidate;
        } catch (Exception failure) {
            // CLI/older packages legitimately have no update source. Never guess one.
            diagnostic.accept("[WARN] update.source: " + failure.getClass().getSimpleName()
                    + ": " + failure.getMessage());
        }
    }

    String description() { return installedName + " (" + installedCode + ")\n" +
            (origin == null ? LauncherText.get(LauncherText.NO_SOURCE) : origin); }
    boolean hasSource() { return origin != null; }
    boolean isPromptShowing() { return promptVisible; }
    private void deliver(String message, boolean available) {
        if (activity.isFinishing() || activity.isDestroyed()) return;
        listener.result(message, available);
        if (!available || promptShown) return;
        promptShown = true;
        promptVisible = true;
        android.app.AlertDialog dialog = new android.app.AlertDialog.Builder(activity)
                .setTitle(LauncherText.get(LauncherText.AVAILABLE))
                .setMessage(message + "\n\n" + LauncherText.get(LauncherText.UPDATE_NOTE))
                .setPositiveButton(LauncherText.get(LauncherText.WEBSITE), (d, w) -> openWebsite())
                .setNegativeButton(DiagnosticText.get(6), null).create();
        dialog.setOnDismissListener(d -> promptVisible = false);
        dialog.show();
    }
    void openWebsite() {
        if (origin == null) return;
        openLink(activity, origin);
    }
    static void openLink(Activity activity, String url) {
        try { activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(url))); }
        catch (android.content.ActivityNotFoundException ignored) { }
    }
    void check(boolean automatic) {
        if (origin == null) { listener.result(LauncherText.get(LauncherText.NO_SOURCE), false); return; }
        android.content.SharedPreferences prefs = activity.getSharedPreferences("ggfm_startup_options", Activity.MODE_PRIVATE);
        long now = System.currentTimeMillis();
        // Every cold launcher check uses fresh metadata, not a day-old cached
        // availability hint which may refer to a rolled-back deployment.
        if (!busy.compareAndSet(false, true)) return;
        listener.result(LauncherText.get(LauncherText.CHECKING), false);
        diagnostic.accept("[INFO] update.check: contacting configured HTTPS deployment; installed=" + installedCode);
        // This thread never participates in the server readiness gate.
        new Thread(() -> {
            HttpsURLConnection connection = null;
            String message = LauncherText.get(LauncherText.OFFLINE);
            boolean available = false;
            try {
                connection = (HttpsURLConnection) new URL(origin + "/api/v1/update").openConnection();
                connection.setConnectTimeout(3000);
                connection.setReadTimeout(3000);
                connection.setInstanceFollowRedirects(false);
                connection.setUseCaches(false);
                connection.setRequestProperty("Accept", "application/json");
                connection.setRequestProperty("User-Agent", "GGFM-Update/1");
                int status = connection.getResponseCode();
                if (status != 200) throw new java.io.IOException("HTTP " + status);
                JSONObject latest;
                try (InputStream stream = connection.getInputStream()) { latest = readJson(stream); }
                org.json.JSONArray offered = latest.optJSONArray("androidAbis");
                String[] offeredAbis = offered == null
                        ? new String[]{latest.optString("androidAbi", "arm64-v8a")}
                        : new String[offered.length()];
                if (offered != null) for (int i = 0; i < offered.length(); i++)
                    offeredAbis[i] = offered.optString(i, "");
                if (latest.getInt("schema") != 1 || !UpdateRules.compatible(activity.getPackageName(), signer,
                        latest.getString("applicationId"), latest.getString("signerSha256"))
                        || !UpdateRules.compatibleAbis(android.os.Build.SUPPORTED_ABIS, offeredAbis)) {
                    prefs.edit().remove("update_latest_code").apply();
                    message = LauncherText.get(LauncherText.MISMATCH);
                    diagnostic.accept("[WARN] update.check: incompatible package/signing identity; rejected");
                } else {
                    long code = latest.getLong("versionCode");
                    if (code <= 0 || code > 2100000000L) throw new java.io.IOException("invalid version");
                    prefs.edit().putLong("update_latest_code", code).apply();
                    available = UpdateRules.newer(installedCode, code);
                    message = LauncherText.get(available ? LauncherText.AVAILABLE : LauncherText.CURRENT) + " (" + code + ")";
                    diagnostic.accept("[INFO] update.check: compatible remote=" + code + " newer=" + available);
                }
            } catch (Exception failure) {
                // Offline, TLS, rate limits, or a removed site must never prevent play.
                diagnostic.accept("[INFO] update.check: " + failure.getClass().getSimpleName()
                        + ": " + failure.getMessage() + "; offline play remains available");
            } finally {
                if (connection != null) connection.disconnect();
                prefs.edit().putLong("update_checked_at", now).apply();
                busy.set(false);
            }
            final String result = message;
            final boolean update = available;
            activity.runOnUiThread(() -> {
                deliver(result, update);
            });
        }, "ggfm-update-check").start();
    }
    private static JSONObject readJson(InputStream stream) throws Exception {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[1024];
        long deadline = android.os.SystemClock.elapsedRealtime() + 6000L;
        for (int count; (count = stream.read(buffer)) != -1;) {
            if (android.os.SystemClock.elapsedRealtime() > deadline) throw new java.io.IOException("update metadata timeout");
            if (output.size() + count > 16 * 1024) throw new java.io.IOException("update metadata too large");
            output.write(buffer, 0, count);
        }
        return new JSONObject(output.toString("UTF-8"));
    }
}
