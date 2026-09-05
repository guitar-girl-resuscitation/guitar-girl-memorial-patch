package org.guitargirlresuscitation.memorial;

/** Resource-free terminal text. Kept independent of Android for layout tests. */
final class TerminalText {
    static long cosmeticDelay(long originalMillis, boolean fast) {
        return fast ? 0L : Math.round(originalMillis * 1.2);
    }

    static String fastStartLabel(java.util.Locale locale) {
        String language = locale.getLanguage();
        if (language.equals("zh")) {
            boolean traditional = locale.getScript().equalsIgnoreCase("Hant")
                    || locale.getCountry().equals("TW") || locale.getCountry().equals("HK");
            return traditional ? "快速啟動" : "快速启动";
        }
        switch (language) {
            case "ko": return "빠른 시작";
            case "ja": return "クイック起動";
            case "vi": return "Khởi động nhanh";
            case "es": return "Inicio rápido";
            case "it": return "Avvio rapido";
            case "id": case "in": return "Mulai cepat";
            case "th": return "เริ่มด่วน";
            case "pt": return "Início rápido";
            case "hi": return "तेज़ शुरुआत";
            default: return "Quick start";
        }
    }
    // Cosmetic commands, never executed by a shell. Keep their pacing separate
    // from real diagnostic output and the server readiness gate.
    static final long COMMAND_OUTPUT_DELAY_MS = 150L;
    static final long NEXT_COMMAND_DELAY_MS = 1100L;
    static final long WELCOME_DELAY_MS = 850L;
    static final long DIVIDER_FRAME_MS = 220L;
    static final long TYPE_FRAME_MS = 18L;
    static final long SPINNER_FRAME_MS = 90L;
    static final int SPINNER_FRAMES = 4;
    static final int PROGRESS_STEPS = 16;
    static final String[][] RITUAL = {
        {"> restore soul --from=memory --verify", "  SCAN     ROOM ............ FOUND\n  memory: a familiar room, a new beginning"},
        {"> mount memory --read-only --target=/home/lily", "  LINK     MEMORY .......... MOUNTED\n  keepsakes: every little moment belongs here"},
        {"> tune guitar --preset=home --check", "  RESTORE  GUITAR .......... TUNED\n  six strings, one song waiting to be played"},
        {"> wake navi --gentle --wait", "  WAKE     NAVI ............ PURR\n  companion: curled up beside the guitar"},
        {"> come back --home=/home/lily --wait", "  HOME     DOOR ............ OPEN\n  AIRISUTEK: the lights are on for you"},
    };

    static String progress(int step) {
        int filled = Math.max(0, Math.min(PROGRESS_STEPS, step));
        StringBuilder line = new StringBuilder("[ ");
        for (int i = 0; i < PROGRESS_STEPS; i++) line.append(i < filled ? '#' : '.');
        return line.append(" ] ").append(100 * filled / PROGRESS_STEPS).append('%').toString();
    }

    static char spinner(int frame) { return "|/-\\".charAt(frame % 4); }

    static String severity(String line) {
        if (line.equals("...") || line.equals("Welcome home, Lily!")) return "WELCOME";
        if (line.contains("[ERROR]") || line.contains("fatal.")) return "ERROR";
        if (line.contains("[WARN]")) return "WARN";
        if (line.contains("[INFO]")) return "INFO";
        return "SCENE";
    }
    private TerminalText() {}
}
