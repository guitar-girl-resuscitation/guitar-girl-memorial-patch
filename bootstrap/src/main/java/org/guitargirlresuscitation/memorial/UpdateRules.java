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
    static boolean compatibleAbi(String[] supported, String offered) {
        if (supported == null || offered == null) return false;
        if (!"arm64-v8a".equals(offered) && !"armeabi-v7a".equals(offered)) return false;
        for (String abi : supported) if (offered.equals(abi)) return true;
        return false;
    }
    static boolean compatibleAbis(String[] supported, String[] offered) {
        if (offered == null) return false;
        for (String abi : offered) if (compatibleAbi(supported, abi)) return true;
        return false;
    }
    private UpdateRules() {}
}
