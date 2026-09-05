package org.guitargirlresuscitation.memorial;

public final class CurrencyTextTest {
    public static void main(String[] args) {
        for (String locale : new String[]{"zh-Hans", "zh_chs", "zh_CN", "zh-SG"})
            if (!CurrencyText.forLocale(locale).title.equals("补充货币")) throw new AssertionError(locale);
        for (String locale : new String[]{"zh-Hant", "zh_cht", "zh_TW", "zh-HK"})
            if (!CurrencyText.forLocale(locale).title.equals("補充貨幣")) throw new AssertionError(locale);
        for (String locale : new String[]{"ko", "en", "ja", "zh-Hans", "zh-Hant", "vi", "es", "it", "id", "th", "pt", "hi"}) {
            CurrencyText words = CurrencyText.forLocale(locale);
            if (words.names.length != 6 || words.title.isEmpty() || words.invalid.isEmpty()
                    || words.multiplier.contains("1k") || words.multiplier.contains("1A")
                    || words.multiplier.contains("100"))
                throw new AssertionError(locale);
            if (!locale.equals("en") && words.title.equals("Add currency")) throw new AssertionError(locale);
        }
        for (String key : CurrencyText.KEYS) {
            for (String good : new String[]{"1", "10", "1000"})
                if (!CurrencyText.validSyntax(key, good)) throw new AssertionError(key + good);
            for (String bad : new String[]{"1A", "1k", "1K", "1+2", "1e3", "-1", "+1", "0", ".5", "1.", " 1", "NaN"})
                if (CurrencyText.validSyntax(key, bad)) throw new AssertionError(key + bad);
            if (CurrencyText.validSyntax(key, "2.5"))
                throw new AssertionError(key + " must reject fractional multipliers");
        }
        System.out.println("PASS: 12 currency locales, Chinese aliases and integer multiplier/count input");
    }
}
