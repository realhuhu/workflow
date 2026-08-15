# 安全策略

## 支持版本

当前仅维护最新的 `0.1.x` 分支。发现漏洞时，请优先使用 GitHub 仓库的私有 Security Advisory
报告，不要在公开 Issue 中披露可利用细节。

## Windows 7 边界

Windows 7 已结束微软安全支持。本项目对 Windows 7 SP1 x64 的承诺是 ABI、PE subsystem 和
Win32 API 兼容，不代表该操作系统或旧版第三方运行时仍可获得上游安全修复。

Windows 7 部署必须使用 SP1 并完整安装系统更新。`win7-compatible` SDK 和测试包均应用本地部署
v142 CRT 与 UCRT；`slim` 包或自行构建的程序仍需安装 KB2999226 和匹配的 v142 Redistributable。
五个 API-set 转发 DLL 只应与应用一起放在可执行文件目录，禁止复制到 `System32` 或替换系统文件。

处理不可信窗口、图片或 OCR 输入时，优先在仍受支持的 Windows 版本、低权限账户和隔离环境中运行。
