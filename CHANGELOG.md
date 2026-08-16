# 变更记录

本项目遵循语义化版本。尚未发布的修改记录在 `Unreleased`。

## Unreleased

## 0.2.0

- 增加 JSON v1 线性工作流解析器；可解析现有 Image/Text Clicker、四种动作和全部 Until，重复运行后返回链条最后一个 Clicker。
- `workflow-test` 增加 C++/JSON 流程来源切换，28 个可视化用例均提供与原链式 API 等价的 JSON。
- 扩展 Browser 测试覆盖：四种可选 Until、三种文字匹配模式、有序选择器、点击锚点、反向拖动、向上滚动、多条件 AND 与执行阶段等待；右侧日志按面板宽度自动换行。
- Browser 测试程序支持一次运行全部 28 × 2 个 C++/JSON 流程，逐项重置和校准、失败后继续并输出批量汇总。
- 稳定 Browser 批量用例中的图片点击、文字消失、模糊文字匹配和双向滚轮场景，消除网页事件、OCR 拆框及异步滚动造成的偶发超时。
- 移除命令行 `workflow_example` 及其专用构建选项，保留 SDK、单元测试和 Browser 测试程序。

## 0.1.3

- 将仓库顶层收口为 `src`、`test`、`example`、`resource`、`tool` 和 `cmake`，删除被版本控制的 IDE 配置。
- 统一本地与 CI 构建输出到 `build/`，测试程序改名为 `workflow-test.exe`。
- Release 统一为 `workflow-sdk` 和 `workflow-test` 两类，并同时提供 `slim` 与 `win7-compatible` 版本。
- SDK 运行包补齐 Qt Core、OpenCV 和 ONNX Runtime 必要 DLL；兼容版额外包含 v142 CRT、UCRT 和 Win7 转发层。

## 0.1.2

- Browser UI Release 拆分为 Windows 7 SP1 兼容版和 Windows 10+ 精简版。
- 兼容版应用本地部署完整 MSVC v142 CRT、UCRT 与五个 API Set 转发 DLL，修复缺少
  `MSVCP140.dll`、`api-ms-win-core-shlwapi-legacy-l1-1-0.dll` 等启动错误。
- CI 分别校验并发布两个 Browser 包；精简版不再携带编译器运行库和 Win7 兼容层。

## 0.1.1

- 保留 `run/finish → _createNext` 链传播，并允许 `ClickerBase` 直接使用图片或文字 RunConfig，混合链不再需要 `dynamic_cast`。
- 增加基于 Qt WebEngine 的 Browser UI 校准台，提供 20 个图片、文字和混合链自动化页面。
- GitHub Release 新增独立 Browser UI 运行包，包含示例 EXE、网页用例、OCR 模型及完整运行时。
- 修复 Clicker 动作重载的隐藏关系、Until 安全向下转换和静态检查告警。
- 扩展 Windows 7 PE/import 审计，使其覆盖 Browser UI 目录中的全部 EXE 和 DLL。

## 0.1.0

- 提供图片匹配、OCR 文字匹配、ROI 裁剪、Until 条件链和鼠标滚轮工作流。
- 按 `core/clickers/untils/matching/platform/support` 组织首方源码。
- 增加标准 CMake 安装、导出和独立消费项目验证。
- 增加 GitHub Actions Windows 构建、测试、制品和标签发布流程。
- 将最低运行目标固定为 Windows 7 SP1 x64，并增加 PE/import 审计。
- 固定 ONNX Runtime 1.13.1，随包构建三个 Win7 API-set 转发 DLL。
