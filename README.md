# workflow

`workflow` 是一个 Windows C++20 自动化工作流框架，保留 Conqueror 的 `Clicker`、`Until` 和
`start → run → finish → next` 执行语义，并增加 OCR 文字匹配、推理前 ROI 裁剪、滚轮操作及图片/文字混合链。

框架负责窗口客户区截图、模板匹配、OCR、条件轮询、鼠标输入、超时和停止控制。调用方负责获取
`HWND`、创建工作线程并保证注入对象的生命周期。

## 依赖

| 依赖 | 要求 |
| --- | --- |
| 平台 | Windows 7 SP1 x64 及以上 |
| 编译器 | MSVC v142，支持 C++20；Visual Studio 2019 或 VS 2022 的 v142 工具集 |
| Qt | 5.15.2 EXACT，`Qt5::Core` |
| OpenCV | 4.10.0 EXACT |
| ONNX Runtime | Windows x64 1.13.1（ORT API 13） |
| RapidOCR | 源码、模型和字典已放入仓库 |

OCR 运行时需要 `det.onnx`、`cls.onnx`、`rec.onnx` 和 `keys.txt`，默认位于
`data/rapidocr/models/`。第三方许可证见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

ONNX Runtime 固定为 1.13.1 是 Windows 7 兼容约束。官方 1.20.1 Windows 二进制会直接导入
`CreateFile2` 和 `GetSystemTimePreciseAsFileTime` 等 Windows 8+ API，不能用于 Windows 7 发布包。
1.13.1 仍使用五个 Windows 7 不提供的 API-set 合同名；项目从源码构建五个只转发到 Windows 7
既有 `Kernel32`/`Advapi32`/`Shlwapi` 函数的兼容 DLL，并由运行时部署函数放到应用目录，不修改系统目录。

## 源码结构

```text
src/
├── workflow.h                 # 公共聚合头
├── core/                      # Env、Segment、枚举
├── clickers/
│   ├── base.h/.cpp            # ClickerBase、Clicker<InitConfig, RunConfig> 与公共动作流程
│   ├── image.h/.cpp           # ImageClicker、ImageInitConfig、ImageRunConfig
│   └── text.h/.cpp            # TextClicker、TextInitConfig、TextRunConfig
├── untils/
│   ├── base.h/.cpp            # Until 轮询、反转、超时、Previous 过滤
│   ├── image.h/.cpp           # 图片 Until 家族
│   └── text.h/.cpp            # 文字 Until 家族与 OCR ROI 解析
├── matching/
│   ├── image.h/.cpp           # 截图门面、模板匹配、ROI、NMS
│   ├── text.h/.cpp            # token 文字匹配和编辑距离
│   └── selector.h/.cpp        # Segment 选择器
├── platform/                  # 平台抽象、Win32 截图与鼠标输入
├── support/                   # OCR、日志、资源路径、停止与计时
└── third_party/rapidocr/       # RapidOCR 第三方实现

examples/                      # 可运行示例
tests/                         # 契约、OCR、图片匹配和 Win32 输入测试
cmake/                         # 安装包、运行时部署、格式化和 Windows manifest
scripts/                       # CI 依赖安装与 Windows 7 PE 导入审计
.github/workflows/             # Windows 构建、测试、制品和标签发布
data/rapidocr/models/          # OCR 模型与字典
licenses/                      # 再分发许可证
CONTRIBUTING.md                # 贡献、验证和提交约定
SECURITY.md                    # 漏洞报告与 Win7 安全边界
```

业务代码通常只需：

```cpp
#include "workflow.h"
```

## 执行环境

`env` 是 `thread_local Env`。每个工作线程必须在创建首个 Clicker 之前独立绑定：

```cpp
std::atomic_bool stopFlag{false};
Emitter emitter;
OcrEngine ocr(QDir(QCoreApplication::applicationDirPath()).filePath("models"));

QString error;
if (!ocr.initialize(&error)) {
    throw std::runtime_error(error.toUtf8().toStdString());
}

Env executionEnv;
executionEnv.hwnd = hwnd;
executionEnv.emitter = &emitter;
executionEnv.stopFlag = &stopFlag;
executionEnv.ocr = &ocr;
executionEnv.resourceRoot = QCoreApplication::applicationDirPath();

env = executionEnv;
```

`platform` 为空时使用内置 Win32 实现；测试或其他输入后端可以注入自定义 `Platform`。文字能力要求
`env.ocr` 非空。`Emitter`、停止标记、Platform 和 OCR Provider 的生命周期必须覆盖整个任务。

## Clicker 类型

`ClickerBase` 是不可实例化的虚基类。`Clicker<InitConfig, RunConfig>` 模板只复用动作实现；业务入口必须实例化：

- `ImageClicker`：图片目标，接受 `ImageInitConfig` 和 `ImageRunConfig`。
- `TextClicker`：文字目标，接受 `TextInitConfig` 和 `TextRunConfig`。

这种结构在编译期阻止把 `TextRunConfig` 传给 `ImageClicker`，也阻止把 `ImageRunConfig` 传给
`TextClicker`。

### 图片入口

```cpp
ImageClicker clicker(
    "login/start.png",
    ImageInitConfig{
        .threshold = 0.92f,
        .timeout = 30,
        .wait = 0,
        .mode = Mode::GRAY,
        .region = QRect(0, 0, 900, 700),
    }
);
```

相对图片路径从 `env.resourceRoot/游戏图片/` 解析；绝对路径直接使用。普通构造函数会在构造期间等待
`wait` 并执行首次匹配。传入缓存 `Segment` 或 `std::vector<Segment>` 的构造函数不会重新截图匹配。

### 文字入口

```cpp
TextClicker clicker(
    "确认",
    TextInitConfig{
        .timeout = 30,
        .mode = Mode::RGB,
        .region = QRect(250, 180, 500, 360),
        .match = TextMatchConfig{
            .match = TextMatch::EXACT,
            .threshold = 0.80f,
            .boxThreshold = 0.50f,
        },
    }
);
```

TextClicker 首次匹配时先截图，再裁剪 `region`，最后调用 OCR。缓存 Segment 构造函数同样不会重新 OCR。

### 初始化配置

| 配置 | 字段 | 默认值 | 含义 |
| --- | --- | --- | --- |
| `ImageInitConfig` | `threshold` | `0.9` | 模板匹配阈值 |
| | `timeout` | `60` | 动作循环超时，秒 |
| | `wait` | `0` | 首次匹配前等待，秒 |
| | `mode` | `GRAY` | 截图和模板匹配模式 |
| | `region` | 空 | 模板匹配 ROI；空表示整个客户区 |
| `TextInitConfig` | `timeout` | `60` | 动作循环超时，秒 |
| | `wait` | `0` | 首次匹配前等待，秒 |
| | `mode` | `RGB` | OCR 截图模式 |
| | `region` | 空 | OCR ROI；空表示整个客户区 |
| | `match` | `{}` | `TextMatchConfig` |
| | `resolvedRegion` | `nullopt` | 内部 ROI 缓存；通常由链传播设置 |

`resolvedRegion == nullopt` 表示尚未解析；`resolvedRegion` 有值但 `QRect` 为空表示真实空交集，必须跳过
截图后的 OCR，不能退化成整窗识别。

## 动作与 RunConfig

两种 Clicker 都支持 `locate`、`click`、`drag`、`scroll`、`founded` 和 `end`。无参动作使用对应
RunConfig 的默认值；带配置动作在编译期要求正确类型。

```cpp
auto next = imageClicker.click(
    ImageRunConfig{
        .selector = positionSelector(SelectorBasis::X_CENTER, SelectorMethod::MAX),
        .finishUntilList = {
            new Text("确认", TextUntilConfig{.match = TextMatch::EXACT}),
        },
        .homing = false,
    },
    0.2f
);
```

| 动作 | 语义 |
| --- | --- |
| `locate(config)` | 选择当前命中并写入 `previousSegment`；不接受 `runUntilList` |
| `click(config, interval, offsetX, offsetY, position)` | 至少点击一次；有 run 条件时循环到全部满足 |
| `drag(config, step, reverse)` | 沿 Y 轴拖动并重新匹配当前目标 |
| `scroll(config, delta, interval, offsetX, offsetY, position)` | 无 run 条件滚动一次；有条件先检查当前页，未满足才滚动 |
| `founded()` | 当前 `targetSegmentList` 是否非空 |
| `end()` | 链式终点，无额外动作 |

`ImageRunConfig` 和 `TextRunConfig` 共有：

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `startWait` | `0` | start 条件前等待 |
| `startUntilList` | 空 | 动作前依次等待的条件 |
| `runUntilList` | 空 | 动作循环退出条件；同轮全部满足才退出 |
| `finishUntilList` | 空 | 动作后依次等待的条件 |
| `finishWait` | `0` | finish 条件前等待 |
| `homing` | `true` | 流程前后是否将鼠标移到客户区 `(0,0)` |

`ImageRunConfig` 额外提供 `selector`，默认 `similaritySelector`。`TextRunConfig` 没有 selector，文字目标
固定使用首个匹配结果。

动作返回 `std::unique_ptr<ClickerBase>`，因为最后一个 run/finish 条件可能改变下一节点类别。
`ClickerBase` 直接接受两种 RunConfig，因此跨图片/文字节点仍可保持原有链式写法，不需要类型转换：

```cpp
imageClicker
    .click(ImageRunConfig{
        .finishUntilList = {new Text("确认")},
    })
    ->click(TextRunConfig{.homing = false})
    ->end();
```

直接操作 `ImageClicker` 或 `TextClicker` 时，编译器仍只接受各自的 RunConfig；进入运行时传播后的
`ClickerBase` 同时提供两个配置重载，以保留 Conqueror 的连续动作 API，并在执行前校验配置与当前
`MatchKind` 一致。

## 执行生命周期和链传播

每次动作都遵循：

1. 接管并校验三个 Until 指针列表。
2. `homing=true` 时移动到 `(0,0)`。
3. 等待 `startWait`，依次执行 start 条件。
4. 执行动作并检查 run 条件。
5. 等待 `finishWait`，依次执行 finish 条件。
6. `_createNext()` 优先使用最后一个 finish 条件，否则使用最后一个 run 条件；都没有时调用 `clone()`。
7. `homing=true` 且未停止时再次移动到 `(0,0)`。

`_createNext()` 读取 Until 的 `kind`、`target`、配置和已缓存 Segment，创建对应的 `ImageClicker` 或
`TextClicker`，不会隐式再次截图、模板匹配或 OCR。

停止标记置位后不再发送新的输入，流程返回当前节点的克隆。同步 `PrintWindow` 调用本身无法被停止标记或
框架超时中断；目标窗口线程挂起时，截图仍可能阻塞。

## Until 条件

| 图片条件 | 文字条件 | 作用 |
| --- | --- | --- |
| `Image` | `Text` | 等待单目标 |
| `AnyImage` | `AnyText` | 按候选顺序等待任一目标 |
| `ImageStable` | `TextStable` | 最佳命中连续 3 次中心相同 |
| `IfImage` | `IfText` | 只检查一次，不阻塞 |
| `IfAnyImage` | `IfAnyText` | 任一候选只检查一次，不阻塞 |

所有 Until 都支持 `onPrevious`、`interval`、`startWait`、`finishWait`、`timeout` 和 `reverse`。
`timeout=-1` 时继承当前 Clicker 的 timeout；`reverse=true` 表示有效匹配结果为“未命中”时满足条件。
截图或 OCR 基础设施错误会抛出，不会被 reverse 当作目标消失。

### ImageUntilConfig

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `onPrevious` | `NONE` | 对命中结果进行相对上一步的空间过滤 |
| `mode` | `GRAY` | 图片匹配模式 |
| `threshold` | `0.9` | 模板相似度阈值 |
| `interval` | `0.1` | 轮询间隔，秒 |
| `startWait` / `finishWait` | `0` | 条件前后等待 |
| `timeout` | `-1` | 条件超时；负值继承 Clicker |
| `reverse` | `false` | 等待图片消失 |
| `region` | 空 | 模板匹配 ROI |

### TextUntilConfig

TextUntilConfig 包含相同的生命周期字段，并增加文字匹配和 OCR 裁剪字段：

| 字段 | 默认值 | 含义 |
| --- | --- | --- |
| `mode` | `RGB` | OCR 截图模式 |
| `threshold` | `0` | 文字识别置信度下限 |
| `match` | `CONTAINS` | `EXACT`、`CONTAINS`、`REGEX` 或 `FUZZY` |
| `caseSensitivity` | 不敏感 | 大小写规则 |
| `normalize` | `true` | 非正则匹配前保留字母数字并规范大小写 |
| `boxThreshold` | `0` | OCR 检测框置信度下限 |
| `maxEditDistance` | `1` | FUZZY 最大编辑距离 |
| `candidates` | 空 | FUZZY 消歧候选 |
| `uniqueNearest` | `true` | 目标必须是唯一最近候选 |
| `region` | 空 | 显式窗口客户区 ROI |
| `cropToPrevious` | `true` | OCR 前应用 `onPrevious` 派生裁剪 |
| `cropPadding` | 空 | 用 `QMargins` 扩张或收缩派生 ROI |

## OCR 推理前裁剪

文字 Until 的逻辑区域为：

```text
窗口客户区
  ∩ 显式 region（非空时）
  ∩ onPrevious 派生区域加 cropPadding（cropToPrevious=true 时）
```

- `LEFT/RIGHT/TOP/DOWN` 使用对应半平面。
- `LEFT_CENTER/RIGHT_CENTER` 同时限制在 previous 的纵向范围。
- `TOP_CENTER/DOWN_CENTER` 同时限制在 previous 的横向范围。
- `INNER` 使用 previous 的矩形。
- 空交集立即返回未命中，不调用 OCR。
- OCR 只接收裁剪后的 BGR 图；token 框和中心会加回 ROI 左上角，恢复为客户区全局坐标。
- `AnyText` 对同一帧只 OCR 一次，再按候选顺序复用 token。
- `cropToPrevious=false` 跳过推理前相对裁剪，但仍执行命中后的 Previous 空间过滤。

底层入口：

```cpp
QRect roi = OCR::resolveRegion(
    screen,
    QRect(200, 100, 600, 400),
    Previous::RIGHT_CENTER,
    previous.get(),
    QMargins(12, 8, 12, 8)
);

if (!roi.isEmpty()) {
    OcrRunResult result = OCR::recognize(screen, roi);
}
```

`OCR::recognize(image, QRect{})` 的空参数表示整张图；条件层会区分“未约束”与“求交后真实为空”。

## Segment 与选择器

Segment 只保存匹配几何和统一评分：

```cpp
Segment(int x, int y, int width, int height, float score);
```

公开字段为 `x`、`y`、`width`、`height`、`score`；派生几何通过 `right()`、`bottom()`、`centerX()`、
`centerY()` 获取。Segment 不保存图片路径、文字、MatchKind 或重复的派生坐标。

选择器：

- `similaritySelector`：最高 score。
- `positionSelector(SelectorBasis, SelectorMethod)`：按 X1/Y1/X2/Y2/中心取最小或最大。
- `randomSelector`：随机目标。
- `orderedRandomSelector(basis, method, top)`：排序后在前 top 个中随机。

`Previous` 过滤使用 `Segment::Axis` 和 `Segment::Relation` 枚举，不使用字符串参数。

## 滚轮和鼠标坐标

- 所有 Segment 和鼠标 API 均使用窗口客户区坐标。
- `WheelDelta == 120`；正数向上，负数向下。
- scroll 锚点支持 `CENTER/LEFT/TOP/RIGHT/DOWN`，然后叠加 offset。
- 有 run 条件时先检查当前页面；已经满足时不会先滚动。
- Win32 实现按一个互斥操作发送 `WM_MOUSEMOVE + WM_MOUSEWHEEL`，wheel 消息携带屏幕坐标。
- 点击、滚轮和拖动会校验客户区边界；拖动终点会裁剪到合法范围。

## Until 所有权

为保留 Conqueror 聚合初始化调用方式，RunConfig 中仍使用 `std::vector<Until*>`。动作入口立即接管所有权并
转换为 `std::unique_ptr`：

```cpp
auto next = imageClicker.click(ImageRunConfig{
    .finishUntilList = {
        new Text("确认", TextUntilConfig{.match = TextMatch::EXACT}),
    },
});
```

- 条件必须通过 `new` 创建，或由 `unique_ptr.release()` 明确转移。
- 不要传栈对象地址，也不要在动作调用后访问或删除条件。
- 同一个指针不能重复出现在任意条件列表中。
- 空指针、重复指针以及 locate 的 run 条件会在输入发生前拒绝，并安全释放已接管对象。

## 混合工作流

图片和文字通过 Until 的结果进行链式切换：

```cpp
auto image = std::make_unique<ImageClicker>(
    "start.png",
    ImageInitConfig{.threshold = 0.92f, .timeout = 30}
);

image
    ->click(ImageRunConfig{
        .finishUntilList = {
            new AnyText(
                {"确认", "继续"},
                TextUntilConfig{
                    .match = TextMatch::EXACT,
                    .region = QRect(250, 180, 500, 360),
                }
            ),
        },
        .homing = false,
    })
    ->scroll(
        TextRunConfig{
            .runUntilList = {
                new Text(
                    "完成",
                    TextUntilConfig{
                        .onPrevious = Previous::DOWN,
                        .match = TextMatch::EXACT,
                        .cropToPrevious = true,
                        .cropPadding = QMargins(0, 8, 0, 8),
                    }
                ),
            },
            .homing = false,
        },
        -WheelDelta,
        0.2f
    )
    ->end();
```

完整环境初始化和命令行 HWND 解析见 [examples/mixed_workflow.cpp](examples/mixed_workflow.cpp)。

## Windows 7 兼容范围

支持目标是 **Windows 7 SP1 x64 及以上**，不支持 Windows 7 RTM。Windows 7 机器需要：

- 安装完整系统更新和 [Universal C Runtime 更新 KB2999226](https://support.microsoft.com/help/2999226)；
- 安装与 v142 工具集对应的 Microsoft Visual C++ 2015–2019 x64 Redistributable；
- 使用本项目固定的 Qt 5.15.2、OpenCV 4.10.0 和 ONNX Runtime 1.13.1 运行时。

Windows 7 已结束微软安全支持；这里的“支持”仅表示二进制兼容目标，不代表操作系统仍能获得安全更新。
面向不可信网络或数据的部署应优先使用受支持的新版 Windows，完整安全边界见 [SECURITY.md](SECURITY.md)。

所有项目目标公开定义 `WINVER=0x0601`、`_WIN32_WINNT=0x0601` 和
`NTDDI_VERSION=0x06010000`。应用目标还应调用：

```cmake
workflow_enable_windows7(your_target)
```

该函数写入 Windows 7–11 manifest，并把 MSVC PE subsystem 最低版本设为 6.01。CI 会检查示例及
Qt、OpenCV、ONNX Runtime DLL 的 PE subsystem 和已知 Windows 8+ 强制导入。GitHub 托管环境没有
Windows 7 虚拟机，因此正式发布前仍应在完整更新的 Windows 7 SP1 x64 实机或虚拟机上执行一次冒烟测试。

## 构建与测试

### 使用 CMake Presets

`CMakePresets.json` 使用 GitHub 当前提供的 Visual Studio 2022 主机和 v142 工具集。先设置依赖：

```powershell
$env:WORKFLOW_QT_ROOT = "D:/Qt/5.15.2/msvc2019_64"
$env:WORKFLOW_OPENCV_DIR = "D:/CLibrary/opencv4100-world/build"
$env:WORKFLOW_OPENCV_BIN_DIR = "D:/CLibrary/opencv4100-world/build/x64/vc16/bin"
$env:WORKFLOW_ONNXRUNTIME_ROOT = "D:/CLibrary/onnxruntime-win-x64-1.13.1"

cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --preset windows-release
```

### Visual Studio 2019

在 **x64 Native Tools Command Prompt for VS 2019** 中：

```powershell
cmake -S . -B build -G "Visual Studio 16 2019" -A x64 `
  -DWORKFLOW_QT_ROOT="D:/Qt/5.15.2/msvc2019_64" `
  -DWORKFLOW_OPENCV_DIR="D:/CLibrary/opencv4100-world/build" `
  -DWORKFLOW_OPENCV_BIN_DIR="D:/CLibrary/opencv4100-world/build/x64/vc16/bin" `
  -DWORKFLOW_ONNXRUNTIME_ROOT="D:/CLibrary/onnxruntime-win-x64-1.13.1"

cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

项目作为顶层工程时默认构建静态库、`mixed_workflow_example` 和测试；通过 `add_subdirectory` 引入时，
示例和测试默认关闭。只构建库：

```powershell
cmake -S . -B build `
  -DWORKFLOW_BUILD_EXAMPLES=OFF `
  -DWORKFLOW_BUILD_TESTS=OFF
cmake --build build --parallel
```

当前测试覆盖 21 组契约，包括：OCR ROI 裁剪和全局坐标回映射、空交集跳过推理、所有 Previous 区域、
AnyText 单次推理、文字匹配模式、图片 NMS、链类型传播、Until 所有权、停止/超时、所有 Until 家族、
scroll 时序以及真实 Win32 wheel 消息边界。

## 安装并供其他项目使用

构建后生成标准 CMake 安装树：

```powershell
cmake --install build --config Release --prefix stage
```

安装树包含公开头文件、`workflow.lib`、`workflow_rapidocr.lib`、ONNX Runtime import/runtime、五个
Win7 API-set 转发 DLL、OCR
模型、许可证以及 `workflowConfig.cmake`。Qt 和 OpenCV 开发包不会被重复打包；消费项目必须提供完全
一致的 Qt 5.15.2 和 OpenCV 4.10.0。

消费项目的 `CMakeLists.txt`：

```cmake
find_package(workflow CONFIG REQUIRED)

add_executable(my_automation main.cpp)
target_link_libraries(my_automation PRIVATE workflow::workflow)
workflow_enable_windows7(my_automation)
workflow_stage_runtime(my_automation)
```

配置消费项目时指定安装包及依赖位置：

```powershell
cmake -S . -B build `
  -Dworkflow_DIR="D:/sdk/workflow/lib/cmake/workflow" `
  -DCMAKE_PREFIX_PATH="D:/Qt/5.15.2/msvc2019_64;D:/CLibrary/opencv4100-world/build"
```

`workflow_stage_runtime()` 会把消费项目所使用的 Qt/OpenCV DLL、包内 ONNX Runtime、OCR 模型和
许可证复制到目标可执行文件旁。仓库中的 `tests/package_consumer` 会在 CI 中从安装树重新配置和链接，
防止只在源码树中可用、安装后失效。

也可以直接通过 `add_subdirectory()` 集成并链接 `workflow::workflow`。

## GitHub CI/CD

[`.github/workflows/windows.yml`](.github/workflows/windows.yml) 在 `windows-2022` 上使用 v142 工具集执行：

1. 下载并校验固定版本依赖；
2. clang-format 22 检查；
3. Release 构建和 CTest；
4. SDK 与 Browser UI 全部运行文件的 Windows 7 PE/import 审计；
5. 安装 SDK并构建独立消费项目；
6. 上传 SDK `workflow-<version>-windows-x64-win7sp1.zip`；
7. 上传两个可直接运行的 Browser UI 示例：
   - `workflow-browser-ui-<version>-windows-x64-win7-compatible.zip`：Windows 7 SP1 兼容版，额外包含
     MSVC v142 CRT、应用本地 UCRT 和五个 API Set 转发 DLL；
   - `workflow-browser-ui-<version>-windows-x64-slim.zip`：Windows 10+ 精简版，仅包含应用、Qt、
     OpenCV、ONNX Runtime、OCR 模型和网页资源，要求系统已安装 Microsoft Visual C++ x64 运行库。

推送 `v*` 标签时还会构建 Debug 库，并通过 GitHub CLI 创建对应 Release。标签应与
`project(workflow VERSION ...)` 保持一致。

## 格式化

项目要求 clang-format 22 或更高版本。本机路径示例：

```powershell
cmake -DWORKFLOW_FORMAT_MODE=fix `
  -DWORKFLOW_CLANG_FORMAT_EXECUTABLE="D:/Tools/clang-format-22/bin/clang-format.exe" `
  -P cmake/Format.cmake

cmake -DWORKFLOW_FORMAT_MODE=check `
  -DWORKFLOW_CLANG_FORMAT_EXECUTABLE="D:/Tools/clang-format-22/bin/clang-format.exe" `
  -P cmake/Format.cmake
```
