# H.264 Analyzer

基于 C++17、Qt 6 Widgets、QOpenGLWidget、FFmpeg 和 CMake 的跨平台 H.264 码流分析器主框架。

当前阶段已集成 FFmpeg 做基础视频文件打开与连续解码预览，但还不包含 H.264 语法树解析逻辑。后续可以在 `src/core` 或新增 `src/bitstream`、`src/parser` 模块中逐步接入 NAL Unit、SPS/PPS、Slice、Frame 等解析能力。

## 功能概览

- `File -> Open` 打开 `.264`、`.h264`、`.es`、`.bin` 等裸码流文件。
- 支持把本地文件拖放到主窗口打开。
- 使用 FFmpeg 在后台线程中打开视频流并解码帧。
- `VideoCanvas` 使用 `sws_scale` 将 YUV/源像素格式转换为 RGBA，并上传为 OpenGL 纹理渲染。
- 使用自研 `H264Parser` 旁路解析 H.264 NALU、SPS、PPS 和 Slice Header，右侧属性树可随左侧帧选择动态刷新。
- `VideoCanvas` 可叠加宏块网格、QP 热力图和运动矢量箭头；当前 MV 数据结构已接入，真实 MV 需要继续解析 slice_data。
- 使用 `QDockWidget` 搭建可停靠布局：
  - 左侧：`FrameListView`，基于 `QTreeWidget`。
  - 中央：`VideoCanvas`，继承 `QOpenGLWidget`，预留视频和矢量叠加渲染接口。
  - 右侧：`PropertyTreeView`，基于 `QTreeWidget`。
  - 底部：`LogDock`，基于 `QPlainTextEdit`。
- `View -> Docks` 可显示/隐藏各停靠面板。
- 使用 `QStandardPaths` 选择跨平台默认打开目录。

## 目录结构

```text
h264_analyzer/
├── CMakeLists.txt
├── README.md
└── src/
    ├── main.cpp
    ├── app/
    │   ├── MainWindow.h
    │   └── MainWindow.cpp
    ├── core/
    │   ├── DecodeWorker.h
    │   ├── DecodeWorker.cpp
    │   ├── FFmpegDecoder.h
    │   ├── FFmpegDecoder.cpp
    │   ├── H264Parser.h
    │   ├── H264Parser.cpp
    │   ├── StreamDocument.h
    │   └── StreamDocument.cpp
    └── ui/
        ├── FrameListView.h
        ├── FrameListView.cpp
        ├── LogDock.h
        ├── LogDock.cpp
        ├── PropertyTreeView.h
        ├── PropertyTreeView.cpp
        ├── VideoCanvas.h
        └── VideoCanvas.cpp
```

模块职责：

- `src/app`：应用级窗口、菜单、工具栏、Dock 布局、拖放和文件打开流程。
- `src/ui`：独立 UI 控件，尽量保持可复用。
- `src/core`：非 UI 的核心数据模型、FFmpeg 解码封装、后台解码 Worker 与后续解析逻辑入口。

## 环境要求

- CMake 3.21 或更高版本。
- Qt 6，安装 `Core`、`Widgets`、`OpenGL`、`OpenGLWidgets` 组件。
- FFmpeg 开发库：`avformat`、`avcodec`、`avutil`、`swscale`。
- C++17 编译器：
  - Windows：MSVC 2022 或 MinGW-w64。
  - macOS：Apple Clang / Xcode Command Line Tools。
  - Linux：GCC 或 Clang。

官方参考：

- Qt 安装与 Qt Online Installer：https://doc.qt.io/qt-6/get-and-install-qt.html
- Qt CMake 使用方式：https://doc.qt.io/qt-6/cmake-get-started.html
- CMake 下载：https://cmake.org/download/
- FFmpeg API 文档：https://ffmpeg.org/doxygen/trunk/
- vcpkg ffmpeg 包：https://vcpkg.io/en/package/ffmpeg.html

## 安装编译环境

### Windows

推荐组合 1：Qt 6 + MSVC 2022。

1. 安装 Visual Studio 2022，并勾选 `Desktop development with C++`。
2. 安装 CMake。可以用 Visual Studio Installer 自带的 CMake，也可以从 CMake 官网安装独立版本。
3. 安装 vcpkg，并安装 FFmpeg：

```powershell
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg.exe install "ffmpeg[avcodec,avformat,swscale]:x64-windows"
```

4. 使用 Qt Online Installer 安装 Qt 6，组件选择类似：
   - `Qt 6.x.x -> MSVC 2022 64-bit`
   - `Qt 6.x.x -> Qt 5 Compatibility Module` 不需要
   - `Qt 6.x.x -> Additional Libraries` 通常不需要
5. 确认 Qt CMake 包路径，例如：

```powershell
$env:CMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
```

推荐组合 2：Qt 6 + MinGW。

1. 使用 Qt Online Installer 安装 Qt 6 的 MinGW 套件，例如 `mingw_64`。
2. 确保 CMake、Ninja 或 MinGW Makefiles 可用。
3. 使用 vcpkg 安装 MinGW 对应 triplet 的 FFmpeg，或安装 MSYS2 的 FFmpeg 开发包。
4. 设置 Qt 路径，例如：

```powershell
$env:CMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
```

### macOS

1. 安装 Xcode Command Line Tools：

```bash
xcode-select --install
```

2. 安装 CMake 和 Qt：

```bash
brew install cmake qt ffmpeg pkg-config
```

3. 如果 CMake 找不到 Qt，设置：

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

### Linux

Ubuntu / Debian 示例：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Fedora 示例：

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel mesa-libGL-devel ffmpeg-devel
```

如果发行版仓库中的 Qt 版本偏旧，可以改用 Qt Online Installer 安装 Qt 6，并设置 `CMAKE_PREFIX_PATH`。

## 构建与运行

推荐使用 Ninja：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行：

```bash
# Windows
.\build\H264Analyzer.exe

# macOS
open build/H264Analyzer.app

# Linux
./build/H264Analyzer
```

如果 CMake 找不到 Qt，请显式传入 Qt 安装前缀：

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<kit>
```

Windows MSVC 示例：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="C:\vcpkg\scripts\buildsystems\vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\msvc2022_64"
cmake --build build --config Debug
.\build\Debug\H264Analyzer.exe
```

Windows MinGW 示例：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="C:\Qt\6.x.x\mingw_64"
cmake --build build
.\build\H264Analyzer.exe
```

## FFmpeg 集成说明

`FFmpegDecoder` 使用以下 FFmpeg API：

- `avformat_open_input` / `avformat_find_stream_info` 打开文件并读取封装信息。
- `av_find_best_stream` 查找视频流。
- `avcodec_alloc_context3` / `avcodec_parameters_to_context` / `avcodec_open2` 初始化解码器。
- `av_read_frame`、`avcodec_send_packet`、`avcodec_receive_frame` 解码视频帧。
- `sws_getCachedContext` / `sws_scale` 在 `VideoCanvas` 中把源帧转换为 RGBA。

跨线程传输时不会直接发送 FFmpeg 内部的 `AVFrame*`。`DecodeWorker` 会立即把解码帧深拷贝到 `DecodedVideoFrame`，再通过 Qt queued signal 发送给 UI 线程。

## 二次开发建议

后续解析功能建议按以下方向扩展：

1. 继续完善 `src/core/H264Parser.*`，补齐 slice_data、CABAC/CAVLC、宏块级 `mb_type`、运动矢量和残差解析。
2. 如果解析器继续变大，可拆出 `src/bitstream`、`src/parser`，分别承载 bit reader、Annex B/AVCC 分割、NAL Unit、SPS、PPS、Slice Header 等逻辑。
3. 在 `src/core` 中维护当前码流文档、帧列表、属性树模型、解码状态和解析日志。
4. 在 `src/ui` 中只展示模型数据，不直接解析码流。
5. `VideoCanvas` 后续可把当前 QPainter 覆盖层迁移到 OpenGL VBO，以支撑大量宏块和运动矢量的高性能绘制。

新增源码后记得同步更新 `CMakeLists.txt` 的 `add_executable` 源文件列表。
