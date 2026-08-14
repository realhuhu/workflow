# 贡献指南

感谢参与 `workflow`。项目当前只接受 Windows x64、MSVC v142、C++20 构建，并保持
Windows 7 SP1 兼容目标。

## 开发环境

准备 README 中固定版本的 Qt、OpenCV 和 ONNX Runtime，设置 `WORKFLOW_*` 环境变量后执行：

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
cmake --build build/windows-v142 --config Release --target workflow_lint
cmake --build build/windows-v142 --config Release --target workflow_win7_audit
```

涉及公共 CMake 接口或头文件时，还必须安装并验证独立消费项目：

```powershell
cmake --install build/windows-v142 --config Release --prefix stage
cmake -S tests/package_consumer -B build/package-consumer `
  -G "Visual Studio 17 2022" -A x64 -T v142 `
  -Dworkflow_DIR="$pwd/stage/lib/cmake/workflow" `
  -DCMAKE_PREFIX_PATH="$env:WORKFLOW_QT_ROOT;$env:WORKFLOW_OPENCV_DIR"
cmake --build build/package-consumer --config Release
ctest --test-dir build/package-consumer -C Release --output-on-failure
```

## 修改约定

- 首方 C++ 代码必须通过 clang-format 22。
- 不要修改 `src/third_party/rapidocr`，除非同步记录上游基准、修改原因和许可证通知。
- OCR 条件必须在推理前裁剪 ROI，并把识别坐标回映射到窗口客户区。
- 不得引入 Windows 8+ 的强制导入；新增 Win32 API 前必须运行 `workflow_win7_audit`。
- 修改公开 API、运行时文件或依赖版本时同步更新 README、CHANGELOG 和安装包测试。

提交 Pull Request 前请说明行为变化、兼容性影响和已执行的测试。
