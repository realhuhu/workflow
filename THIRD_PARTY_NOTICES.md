# 第三方组件说明

MIT 许可证只覆盖本仓库原创代码和文档，不替代下列组件自己的许可证。本仓库内置 RapidOCR C++ 封装源码与 OCR 模型，但不内置 Qt、OpenCV 或 ONNX Runtime；本地构建会把外部动态库和内置模型复制到输出目录。发布二进制包的人有责任同时满足对应许可证和通知要求。

| 组件 | 本项目使用范围 | 许可证/说明 |
| --- | --- | --- |
| Qt 5.15 | Core | 可采用 LGPLv3、GPL 或商业许可；本项目使用动态链接，发布者应按自己选择的 Qt 许可履行义务 |
| OpenCV 4.10.0 | 截图矩阵、图像处理、图像编码 | Apache License 2.0 |
| ONNX Runtime 1.13.1 | OCR 模型推理；固定该版本以支持 Windows 7 SP1 | MIT License；官方发行包还附带 `ThirdPartyNotices.txt` |
| RapidAI/RapidOCR | OCR 检测、方向、识别模型与实现 | Apache License 2.0；本项目内置 C++/ONNX Runtime 封装所需源码和模型 |
| realhuhu/rapidOcr | 对 RapidOCR 的 C++ 封装 | 由本项目作者封装；来源和基准 commit 记录在 `src/third_party/rapidocr/README.md` |
| Clipper | rapidOcr 内的多边形处理 | Boost Software License 1.0；全文见 `licenses/BOOST-1.0.txt` |
| OCR 模型 | `det.onnx`、`cls.onnx`、`rec.onnx`、字典 | 随 RapidOCR 封装提交，按 RapidOCR 上游 Apache-2.0 许可证及模型来源说明使用 |
