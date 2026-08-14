# RapidOCR C++ 封装源码

本目录是项目内置的 RapidOCR C++/ONNX Runtime 封装，用于静态链接 OCR 能力，避免要求使用者另外准备 `rapid_ocr.dll`。

项目发布包使用 ONNX Runtime 1.13.1（ORT API 13）；这是 Windows 7 SP1 兼容构建的固定 ABI，不能直接
替换为导入 Windows 8+ API 的新版官方 Windows DLL。

- OCR 上游：[RapidAI/RapidOCR](https://github.com/RapidAI/RapidOCR)
- 本项目采用的 C++ 封装来源：[realhuhu/rapidOcr](https://github.com/realhuhu/rapidOcr)，基准 commit `60ff0d8a6882229b8d06494176998d80f40f0e28`
- 上游许可证：Apache License 2.0，全文见本目录的 `LICENSE`
- `clipper.cpp/.hpp`：Angus Johnson Clipper 6.4.2，Boost Software License 1.0，全文见 `../../../licenses/BOOST-1.0.txt`

相对封装仓库基准版本，本项目增加 `OCR_STATIC` 导出宏分支，由顶层 CMake 直接编译为静态库，并修正 MSVC 报告的整数宽度与格式化警告。`workflow` 还初始化了 ONNX session 指针、让 Windows 模型与字典路径按 UTF-8 解码，并让字典打开失败通过异常上报；相关修改文件顶部带有显著说明。OCR 模型放在 `data/rapidocr/models`，不会混入 C++ 源码目录。
