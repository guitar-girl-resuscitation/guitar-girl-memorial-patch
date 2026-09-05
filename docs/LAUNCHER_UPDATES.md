# Launcher pages and deployment provenance

The launcher has three pages: Home (terminal, quick startup, Start), Saves
(the existing import/export controller), and About (repositories and updates).
All authored controls use the existing twelve-language locale set; diagnostics
remain English. Import is never triggered by opening the launcher.

Patcher injects `assets/ggfm/update-source.json` with schema 1, HTTPS origin,
applicationId, signerSha256 and versionCode. Missing origin disables checking.
The installed package, certificate and version must agree with this file.
The checker only contacts that origin's `/api/v1/update`, with normal platform
TLS validation, no redirects, bounded response/time, and no game/save payload.
Only a larger Android version for the same application and signer is offered.
Network failure does not block playing offline. The browser opens the originating
Patcher, not a server-provided arbitrary URL. No automatic installation occurs.

Runtime releases contain the matched, compiled Server and its font notice, plus
Patch's own runtime and licensed Dobby dependency. This enables deployments to
fetch a coherent runtime without rebuilding Android native libraries. No game
binary, asset bundle, extracted master table, save or signing key is included.

## Verification boundary

Java tests cover locale completeness, package/signature checks and version rules.
Bootstrap DEX compilation validates the Android APIs. These are not proof of
network delivery or device UI behavior. The client's online update check is
deliberately outside the current device acceptance scope; the deployment
supervisor's automatic update, prebuild and rollback are in scope.

## 中文说明

启动页分为主页、存档、关于三个分区。导入导出继续使用原来的控制器，
打开应用不会主动弹出导入框。终端诊断保留英文，控件覆盖现有十二种语言。

安装包记录产出它的部署站点，只向该 HTTPS 站点检查更新。包名、签名和
版本号必须匹配；仅提示更高版本，用户仍需回到原站点重新打包并覆盖安装。
断网不影响离线游戏，不自动安装、不上传存档。当前暂不实测客户端联网检查；
Patcher 部署端自动更新、重新预构建和失败回退必须测试。
