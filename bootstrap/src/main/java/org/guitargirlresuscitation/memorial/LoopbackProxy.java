package org.guitargirlresuscitation.memorial;

import android.system.ErrnoException;
import android.system.Os;
import java.util.regex.Pattern;

final class LoopbackProxy {
    // The embedded server advertises this numeric address for every game request.
    private static final String HOST = "127.0.0.1";

    static String includeLoopback(String existing, String separator) {
        if (existing == null || existing.isEmpty()) return HOST;
        for (String host : existing.split(Pattern.quote(separator))) {
            if (HOST.equals(host.trim()) || "*".equals(host.trim())) return existing;
        }
        return existing + separator + HOST;
    }

    static void configure() throws ErrnoException {
        // UnityPlayer.getNetworkProxySettings reads this property for HTTP and HTTPS.
        System.setProperty("http.nonProxyHosts",
                includeLoopback(System.getProperty("http.nonProxyHosts"), "|"));
        // Unity's native transport and libcurl consume comma-separated exclusions.
        for (String name : new String[]{"no_proxy", "NO_PROXY", "UNITY_NOPROXY"}) {
            String existing = Os.getenv(name);
            if (existing == null || existing.isEmpty()) {
                existing = Os.getenv("no_proxy".equals(name) ? "NO_PROXY" : "no_proxy");
            }
            Os.setenv(name, includeLoopback(existing, ","), true);
        }
    }

    private LoopbackProxy() {}
}
