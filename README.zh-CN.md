# Guitar Girl Memorial Patch

[English](README.md) · 简体中文

**Guitar Girl Fan Memorial Build** 的客户端集成。本仓库仅包含我们自己的代码和有校验的变换规则，不是游戏资源分发，也不是已经修改好的 APK。

正式独立版**不需要 Root 或 LSPosed**。LSPosed 只是共享 native core 的开发调试适配器，不是正式分发形态。

## 三个仓库分别做什么

| 仓库 | 职责 |
| --- | --- |
| [Server](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-server) | Rust 玩法、协议、SQLite 存档与 Android 内建服务端库 |
| [Patch](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch) | 自有客户端集成、纪念版界面、身份隔离与有校验的变换规则 |
| [Patcher](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patcher) | CLI / 网页打包、原包验证、资源提取、签名和下载 |

游戏运行时，修改后的 Unity 客户端通过带认证的本机回环连接内建 Rust 服务端。补丁网站**不是游戏服务器**，游玩时不需要它持续在线。

## Patch 负责什么

- 在 Unity 启动前拉起内建服务端，提供诊断开屏和显式启动按钮。
- 将游戏请求连接到动态本机端点，附加会话令牌、单调请求序号和设备时间。
- 按 USN 隔离与存档有关的客户端偏好键；语言、音量等安装级设置单独保留。
- 集成游戏设置中的纪念版入口、本地化公告、绝版物品邮件、补充资源、通行证选择和存档管理。
- 按与 Server 相同的 policy 修改客户端公式，使预览、费用与结算一致。
- 将退役 SDK 初始化/回调替换为本地流程，不依赖原广告、支付或在线登录基础设施。
- 任何变换前验证原包是否属于受支持构建。

正式运行时使用独立 loader 和固定版本 Dobby。LSPosed 适配器复用挂钩核心，不应单独复制一套玩法规则。

## 玩家可见功能

纪念版设置按列表展示可发放的绝版服装/吉他、拥有状态，并以一封邮件一个物品的方式发放。已拥有或已在待领邮件中的唯一物品不可重复发送。补充资源使用整数输入：赞、音符、粉丝按游戏的奖励倍率计算，巧克力和糖果按具体个数计算；奖励增益沿用游戏正常奖励链路。

通行证选择直接展示期数，手动选择会重设该档每日轮换锚点。切档必须等待重启/标题登录边界，不会在当前 Unity 会话中直接替换身份。

开屏提供诊断以及存档导入/导出。导入会覆盖当前数据库，想保留现有内容请先导出；不承诺兼容旧实验存档或官方云存档格式。

纪念版 UI 和公告覆盖客户端已有的 12 种语言，未知语言回退英文。真实诊断日志有意保留英文。

## 支持的原包与资源边界

当前仅支持 **Guitar Girl 8.0.0、Android ARM64**，外层 XAPK 的 SHA-256 必须为：

```text
E395AD8A0BF09EA9425D7751388D61C31E9B63411640A716432AC97940BB9FAC
```

版本号一样并不代表兼容。[兼容清单](compatibility/8.0.0.json) 还会检查所需 split 及相关二进制/数据指纹。未知原包直接拒绝，7.0.0 不是替代输入版本。

公开内容可以包含自有实现、哈希、方法描述符、偏移、短断言模式和结构化变换，不包含原版资源或完整反编译函数。Patcher 从用户原包生成主表，只在私有打包过程中修改游戏。

## 目录说明

| 路径 | 内容 |
| --- | --- |
| `bootstrap/` | 自有 Android 启动器、开屏和适配层 |
| `native-core/` | 共享 native 集成核心与 loader |
| `compatibility/` | 特定构建的验证与变换清单 |
| `policy/` | 共享的版本化数值规则 |
| `localization/` | 纪念版自有翻译 |
| `tools/` | 运行时构建、变换与校验工具 |
| `tests/` | 静态/契约回归测试 |

## 构建与验证

以 Release workflow 的工具链为准：Java 21、Android platform 35 / build-tools 35.0.0、NDK 27.3.13750724、CMake 3.22.1、Python 和 Git。解析 Release 依赖还使用 GitHub CLI，CI 从环境变量提供令牌。

```sh
git clone https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch
cd guitar-girl-memorial-patch
python -m unittest discover -s tests -v
python tools/verify_policy.py policy/memorial-policy.v1.json
python tools/build_runtime.py --sdk "$ANDROID_HOME" --java-home "$JAVA_HOME" --server-tag nightly
```

构建会解析并校验 Server、Dobby 依赖。文件缺失、指纹错误、ABI / policy 不符均直接失败。仓库不内置 Dobby 或 Server 二进制。

输出位于 `build/release-runtime/`，包括 `classes.dex`、bootstrap / Dobby 动态库及 `dependencies.json`。[Patch Release](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patch/releases) 将运行时组件打成 `ggfm-patch-android-arm64.zip`，**它不能作为游戏直接安装**。

生成游戏安装包时，应使用 [Patcher](https://github.com/guitar-girl-resuscitation/guitar-girl-memorial-patcher/blob/main/README.zh-CN.md)，配合同一准确 Patch 提交、对应运行时包和 `dependencies.json` 指定的 Server。不要混用独立变化的 Nightly。

## 规则与开发流程

[纪念版 policy](policy/memorial-policy.v1.json) 具有版本号，并在两端校验指纹。修改公式应同步 Server 和客户端、增加回归测试，再验证真正打包后的游戏。静态方法存在或编译成功，不等于游戏行为已验证。

客户端持久化变更必须保持 USN 隔离，包括 Gallery、教程、聊天及迟到的保存请求。旧会话结束之前，不允许暴露新存档的本地键。

main 构建成功后替换唯一 Nightly，标签发布版本化 Release。为保证可复现，记录准确提交、产物摘要和依赖信息。

## 进一步阅读

- [运行时集成与构建契约](docs/RUNTIME.md)
- [客户端存档命名空间](docs/CLIENT_SLOT_NAMESPACE.md)
- [离线购买订单处理](docs/OFFLINE_PENDING_ORDERS.md)
- [离线价格查询](docs/OFFLINE_PRICE_QUERY.md)

## 项目边界、贡献与许可

这是非官方的粉丝纪念版与互操作项目，与原开发商、发行商没有官方关联或背书。不恢复官方账号、云存档、支付或已退役的线上服务。部分历史服务端专有数值采用纪念版兼容值，不宣称完整复现原服全部数据。

项目代码采用 [AGPL-3.0-or-later](LICENSE)，第三方组件保留各自许可；该许可不覆盖原游戏。请只使用你有权使用的原始安装包。

请勿提交 APK/XAPK、AssetBundle、原版 DEX/IL2CPP 二进制、完整反编译导出、抓取的专有主表、私人存档或签名密钥。反馈问题请提供组件版本/提交、章节、复现步骤和脱敏诊断日志。修改行为时补充契约/回归测试，并同步维护两种语言的 README。
