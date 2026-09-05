package org.guitargirlresuscitation.memorial;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.Scanner;
import java.util.TimeZone;
import org.json.JSONObject;

/** Package-private loopback transport used by the in-game NGUI memorial menu. */
final class MemorialAdminClient {
    static void showPage(String title, String[] labels, String close) {
        MemorialPanel.show(title, labels, close);
    }
    static void showMessage(String title, String body, String close) {
        MemorialPanel.message(title, body, close);
    }
    static void showList(String title, String[] labels, boolean[] enabled,
                         String back, String close, long generation) {
        MemorialPanel.showList(title, labels, enabled, back, close, generation);
    }
    static void dismissPage() { MemorialPanel.dismiss(); }
    static void showCurrencyList(String locale, long generation) {
        CurrencyText text = CurrencyText.forLocale(locale);
        MemorialPanel.showList(text.title, text.names, new boolean[]{true,true,true,true,true,true},
                text.back, text.close, generation);
    }
    static void showCurrencyInput(String currency, String locale, long generation) {
        MemorialPanel.currencyInput(currency, CurrencyText.forLocale(locale), generation);
    }
    /** Identity comes from the active SQLite slot, never from an old PlayerPrefs key. */
    static String localMemberId(long expectedUsn) {
        if (expectedUsn <= 0) return "";
        String response = request("GET", "/memorial/v1/session", "", 0);
        if (!response.startsWith("200\n")) return "";
        try {
            JSONObject session = new JSONObject(response.substring(4));
            if (session.getLong("usn") != expectedUsn) return "";
            String memberId = session.getString("userId");
            return memberId.matches("[1-9][0-9]{0,18}") ? memberId : "";
        } catch (Exception error) {
            return "";
        }
    }
    private static String endpoint;
    private static String capability;

    static synchronized void initialize(String nextEndpoint, String nextCapability) {
        endpoint = nextEndpoint;
        capability = nextCapability;
    }

    /** Returns an ASCII status line followed by the unmodified response body. */
    static synchronized String request(String method, String path, String body, long requestSeq) {
        if (endpoint == null || capability == null || path == null || !path.startsWith("/")) {
            return "599\nmemorial transport is not initialized";
        }
        HttpURLConnection connection = null;
        try {
            connection = (HttpURLConnection) new URL(endpoint + path).openConnection();
            connection.setConnectTimeout(2_000);
            connection.setReadTimeout(3_000);
            connection.setUseCaches(false);
            connection.setRequestMethod(method);
            connection.setRequestProperty("X-GGFM-Session", capability);
            if (!"GET".equals(method)) {
                long nowMillis = System.currentTimeMillis();
                connection.setRequestProperty("X-GGFM-Request-Seq", Long.toString(requestSeq));
                connection.setRequestProperty("X-GGFM-Device-Time", Long.toString(nowMillis / 1000L));
                connection.setRequestProperty(
                        "X-GGFM-UTC-Offset",
                        Integer.toString(TimeZone.getDefault().getOffset(nowMillis) / 60_000));
            }
            if (body != null && !body.isEmpty()) {
                byte[] payload = body.getBytes(StandardCharsets.UTF_8);
                connection.setDoOutput(true);
                connection.setFixedLengthStreamingMode(payload.length);
                connection.setRequestProperty("Content-Type", "application/json; charset=utf-8");
                connection.getOutputStream().write(payload);
            }
            int status = connection.getResponseCode();
            InputStream stream = status >= 400 ? connection.getErrorStream() : connection.getInputStream();
            String response = "";
            if (stream != null) {
                try (InputStream input = stream;
                     Scanner scanner = new Scanner(input, StandardCharsets.UTF_8.name())) {
                    response = scanner.useDelimiter("\\A").hasNext() ? scanner.next() : "";
                }
            }
            return status + "\n" + response;
        } catch (Exception error) {
            return "599\n" + error.getClass().getSimpleName() + ": " + error.getMessage();
        } finally {
            if (connection != null) connection.disconnect();
        }
    }

    private MemorialAdminClient() {}
}
