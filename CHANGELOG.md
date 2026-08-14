# 变更记录

本项目遵循语义化版本。尚未发布的修改记录在 `Unreleased`。

## Unreleased

## 0.1.0

- 提供图片匹配、OCR 文字匹配、ROI 裁剪、Until 条件链和鼠标滚轮工作流。
- 按 `core/clickers/untils/matching/platform/support` 组织首方源码。
- 增加标准 CMake 安装、导出和独立消费项目验证。
- 增加 GitHub Actions Windows 构建、测试、制品和标签发布流程。
- 将最低运行目标固定为 Windows 7 SP1 x64，并增加 PE/import 审计。
- 固定 ONNX Runtime 1.13.1，随包构建三个 Win7 API-set 转发 DLL。
