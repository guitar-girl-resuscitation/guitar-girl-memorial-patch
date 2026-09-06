package org.guitargirlresuscitation.memorial;
import java.util.Locale;
public final class LauncherUpdateTest {
    private static void check(boolean condition) { if (!condition) throw new AssertionError(); }
    public static void main(String[] args) {
        check(UpdateRules.newer(800001, 800009));
        check(!UpdateRules.newer(800009, 800009));
        check(!UpdateRules.newer(800009, 800001));
        check(!UpdateRules.newer(800009, Long.MAX_VALUE));
        check(UpdateRules.compatible("same", "AABB", "same", "aa:bb"));
        check(!UpdateRules.compatible("same", "AABB", "different", "AABB"));
        check(!UpdateRules.compatible("same", "AABB", "same", "CCDD"));
        check(UpdateRules.compatibleAbi(new String[]{"armeabi-v7a"}, "armeabi-v7a"));
        check(!UpdateRules.compatibleAbi(new String[]{"armeabi-v7a"}, "arm64-v8a"));
        check(UpdateRules.compatibleAbi(new String[]{"arm64-v8a", "armeabi-v7a"}, "armeabi-v7a"));
        check(!UpdateRules.compatibleAbi(new String[]{"arm64-v8a"}, "armeabi-v7a"));
        check(!UpdateRules.compatibleAbi(new String[]{"x86"}, "x86"));
        check(UpdateRules.compatibleAbis(new String[]{"armeabi-v7a"}, new String[]{"arm64-v8a", "armeabi-v7a"}));
        check(UpdateRules.compatibleAbis(new String[]{"arm64-v8a"}, new String[]{"arm64-v8a", "armeabi-v7a"}));
        check(!UpdateRules.compatibleAbis(new String[]{"x86"}, new String[]{"arm64-v8a", "armeabi-v7a"}));
        for (String locale : new String[]{"en","zh-CN","zh-TW","ja","ko","vi","es","it","id","th","pt","hi","fr"}) {
            Locale.setDefault(Locale.forLanguageTag(locale));
            for (int key = 0; key <= LauncherText.REPOSITORIES; key++) check(!LauncherText.get(key).trim().isEmpty());
            for (int key = 0; key < 7; key++) check(!DiagnosticText.get(key).trim().isEmpty());
        }
        System.out.println("Launcher localization and update identity rules passed");
    }
}
