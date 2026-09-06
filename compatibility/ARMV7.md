# ARMv7 source profile

`8.0.0-armv7.json` is an experimental, separately verified 32-bit profile.
It is **not** the ARM64 APKPure XAPK renamed to ARMv7. The source XAPK hash
identifies the private integration-test container assembled from the three
original split APKs whose individual hashes are in the manifest. It is not
claimed to be a publicly downloadable APKPure archive. No game files are
distributed by this repository. Other containers must be audited independently;
do not disable hash validation to accept them.

The ARMv7 IL2CPP methods and object fields were mapped independently. There
are 66 hooks, 10 dependencies and 40 field offsets. PlayerPrefs.Save stays
native; key isolation is applied at the accessors. ARM32 branch relocation
requires the checked Dobby correction in `tools/fix_dobby_arm.py`.

The real Patcher pipeline has produced a test-signed package which ran on a
Samsung SM-A336E as an actual `armeabi-v7a` process, through startup, local
login, opening chat, notices and the game room. This is not a claim that every
gameplay screen or every 32-bit-only handset has passed acceptance.

CI builds ARM64 then ARMv7 sequentially and releases separate runtime archives.
These archives contain authored runtime code and licensed dependencies, not
original game libraries. A deployment chooses its ABI via its compatibility
manifest. Automatic updates retain that ABI and require matching Server/Patch
libraries. An ARM64-only source cannot produce a working ARMv7 game.

The Patcher can also combine the approved ARM64 source with the separately
verified original ARMv7 native split. Both runtime archives must have identical
Patch/Server commits, policy and bootstrap DEX. The resulting XAPK shares its
base/assets and includes both ABI splits; without that private supplemental
input the output remains single-ABI. The original split is never published here.
Keep the deployment application ID and signing certificate unchanged when
updating; never install integration-test signing over a published build.
