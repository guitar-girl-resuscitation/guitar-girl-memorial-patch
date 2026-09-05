package org.guitargirlresuscitation.memorial;

public final class TerminalTextTest {
    public static void main(String[] args) {
        if (TerminalText.cosmeticDelay(1000, false) != 1200 || TerminalText.cosmeticDelay(1100, false) != 1320
                || TerminalText.cosmeticDelay(1000, true) != 0) throw new AssertionError("quick start pacing");
        if (!TerminalText.severity("[INFO] database open").equals("INFO")) throw new AssertionError();
        if (!TerminalText.severity("[WARN] absent optional resource").equals("WARN")) throw new AssertionError();
        if (!TerminalText.severity("[ERROR] failed").equals("ERROR")) throw new AssertionError();
        if (!TerminalText.severity("fatal.master_data bad").equals("ERROR")) throw new AssertionError();
        if (!TerminalText.severity("[SCENE] > come back").equals("SCENE")) throw new AssertionError();
        if (!TerminalText.severity("...").equals("WELCOME")) throw new AssertionError();
        if (!TerminalText.severity("Welcome home, Lily!").equals("WELCOME")) throw new AssertionError();
        if (TerminalText.COMMAND_OUTPUT_DELAY_MS >= 350L
                || TerminalText.NEXT_COMMAND_DELAY_MS <= 650L)
            throw new AssertionError("ritual must reply quickly and pause between commands");
        if (TerminalText.DIVIDER_FRAME_MS <= 0
                || TerminalText.WELCOME_DELAY_MS <= 2 * TerminalText.DIVIDER_FRAME_MS)
            throw new AssertionError("divider must have visible frames and a final pause");
        for (String[] pair : TerminalText.RITUAL) {
            if (pair.length != 2 || !pair[0].startsWith("> ") || !pair[0].contains("--")
                    || !pair[1].startsWith("  ")) throw new AssertionError("command/output pair");
        }
        if (TerminalText.progress(0).contains("#") || !TerminalText.progress(16).endsWith("100%"))
            throw new AssertionError("progress endpoints");
        for (int i=0; i<=16; i++) {
            String bar=TerminalText.progress(i);
            if (bar.indexOf(']') != 19 || bar.chars().filter(c -> c=='#').count()!=i)
                throw new AssertionError("progress width/monotonicity");
        }
        if (TerminalText.TYPE_FRAME_MS > 30 || TerminalText.SPINNER_FRAMES * TerminalText.SPINNER_FRAME_MS > 600)
            throw new AssertionError("typing/spinner must stay quick");
        for (String tag : new String[]{"en", "ko", "ja", "zh-Hans", "zh-Hant", "vi", "es", "it", "id", "th", "pt", "hi"}) {
            String quick = TerminalText.fastStartLabel(java.util.Locale.forLanguageTag(tag));
            if (quick.contains("(") || quick.contains("（")) throw new AssertionError("quick start label must have no parenthetical explanation");
            if (quick.isEmpty() || (!tag.equals("en") && quick.startsWith("Quick start"))) throw new AssertionError("quick start locale " + tag);
            SaveTransferText words=SaveTransferText.forLocale(java.util.Locale.forLanguageTag(tag));
            if(words.export.isEmpty() || words.importSave.isEmpty() || words.warning.isEmpty())
                throw new AssertionError("transfer locale " + tag);
            if (!tag.equals("en") && words.export.equals("Export database")) throw new AssertionError(tag);
        }
        if (!TerminalText.RITUAL[TerminalText.RITUAL.length - 1][0].startsWith("> come back"))
            throw new AssertionError("last command changed");
        System.out.println("PASS: ritual command pairs, timing and diagnostic/welcome levels");
    }
}
