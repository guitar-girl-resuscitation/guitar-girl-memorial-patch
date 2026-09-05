package org.guitargirlresuscitation.memorial;

/** Update decisions are independent of Android, game assets and saves. */
final class UpdateRules {
    static boolean compatible(String installedPackage, String installedSigner,
            String offeredPackage, String offeredSigner) {
        return installedPackage != null && installedSigner != null && offeredSigner != null
                && installedPackage.equals(offeredPackage)
                && installedSigner.replace(":", "").equalsIgnoreCase(offeredSigner.replace(":", ""));
    }
    static boolean newer(long installed, long offered) {
        return installed > 0 && offered > installed && offered <= 2100000000L;
    }
    private UpdateRules() {}
}
