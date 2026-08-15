# Workflow 浏览器自动化校准台

这是一个完全离线的 Qt WebEngine 可视化测试应用。左侧是固定为 `1000 × 800` 的网页客户区，右侧是
20 个测试用例、运行控制和框架日志。点击“运行当前用例”后，应用会在工作线程中创建真实的
`ImageClicker` / `TextClicker`，并把左侧 `QWebEngineView::winId()` 作为目标 HWND。

## 构建和运行

浏览器测试台在项目作为顶层工程时默认启用，也可以显式指定：

```powershell
cmake --preset windows-msvc -DWORKFLOW_BUILD_BROWSER_HARNESS=ON
cmake --build build/windows-v142 --config Release --target workflow_browser_harness
& "build/windows-v142/tests/browser/Release/workflow-browser-harness.exe"
```

构建后会自动：

- 把 `web/` 复制到可执行文件旁的 `browser-test/`；
- 使用 `workflow_stage_runtime()` 部署 OCR 模型、OpenCV、ONNX Runtime 和许可证；
- 使用 `windeployqt` 部署 Qt WebEngine 进程、资源和 DLL；
- 首次启动时生成四张无抗锯齿的 48 × 48 PNG 模板到
  `browser-test/游戏图片/fixtures/`，网页和 OpenCV 使用同一份文件。

GitHub Release 中的 `workflow-browser-ui-<version>-windows-x64-win7sp1.zip` 已包含上述运行文件，解压后
直接启动 `workflow-browser-harness.exe`。Windows 7 仍需安装 KB2999226 和 Microsoft Visual C++
2015–2019 x64 Redistributable。Qt 采用动态链接，Qt LGPL/GPL 许可证全文位于 `licenses/`。

## DPI 与缩放约束

网页本身无法修改操作系统 DPI，所以测试台使用多层约束建立确定性的截图坐标系：

1. 启动 `QApplication` 前调用 Windows 7 可用的 `SetProcessDPIAware()`；
2. 禁用 Qt 自动高 DPI 缩放并固定 96 DPI；
3. 给 Chromium 设置 `--force-device-scale-factor=1` 和 sRGB 色彩配置；
4. `QWebEngineView` 固定为 `1000 × 800`，每次加载后把 zoom factor 恢复为 `1.0`；
5. 每个页面运行前检查 `devicePixelRatio == 1`、Visual Viewport scale 为 `1`、viewport 精确为
   `1000 × 800`。任一条件不满足时页面会被红色校准层锁定，右侧运行按钮保持禁用。

目标模板全部使用整数坐标、原生尺寸 PNG 和无抗锯齿绘制。OCR 目标使用高对比、大字号文本；OCR
测试验证语义匹配和裁剪坐标，不要求不同 Windows 字体栅格化结果逐像素一致。

## 20 个二级页面

| 编号 | 页面 | 框架能力 | 成功效果 |
| --- | --- | --- | --- |
| 01 | `static-image` | `ImageClicker::click` | 点击 A 后出现 B |
| 02 | `image-selector` | `positionSelector` | 只点击最右目标 |
| 03 | `any-image` | `AnyImage` | 候选 C 优先于 B |
| 04 | `delayed-image` | `Image` | 等待延迟图片 |
| 05 | `disappearing-image` | `reverse=true` | 图片消失后继续 |
| 06 | `stable-image` | `ImageStable` | 连续三次位置稳定 |
| 07 | `image-region` | 图片 ROI | 忽略区域外同图 |
| 08 | `image-threshold` | 相似度阈值 | 排除近似变体 |
| 09 | `static-text` | `TextClicker::click` | OCR 点击 `CONFIRM` |
| 10 | `any-text` | `AnyText` | 一次 OCR、候选顺序优先 |
| 11 | `delayed-text` | `Text` | 等待 `READY` |
| 12 | `disappearing-text` | 文字反向条件 | `PROCESSING` 消失后继续 |
| 13 | `stable-text` | `TextStable` | 文字框连续三次稳定 |
| 14 | `text-region` | OCR 推理前 ROI | 只点击区域内文字 |
| 15 | `previous-relation` | `RIGHT_CENTER` | 从图片 Segment 派生文字 ROI |
| 16 | `repeat-click` | click `runUntil` | 第三次点击后终止 |
| 17 | `drag` | 纵向拖动与重匹配 | 进入投放区后终止 |
| 18 | `scroll` | wheel + Text Until | 滚动到底部后终止 |
| 19 | `hidden-layer` | 遮挡与延迟匹配 | 遮挡消失后点击目标 |
| 20 | `mixed-workflow` | 图片 → 文字 → 图片 | 原链式 API 完成三步操作 |

每个页面都公开一个只用于测试台控制的接口：

```javascript
window.workflowFixture.environment(); // 当前 DPR、缩放和 viewport
window.workflowFixture.reset();       // 清理计时器并恢复初始状态
window.workflowFixture.state();       // 当前可视化状态
```

CTest 中的 `workflow.browser-fixtures` 会校验页面数量、文件名、`data-case`、catalog 条目和对应 renderer，
避免新增或重命名页面后留下不完整的测试入口。
