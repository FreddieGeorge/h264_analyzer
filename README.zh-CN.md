# H.264 Analyzer

[English](README.md) | 简体中文

H.264 Analyzer 是一个跨平台桌面 H.264 码流分析器项目，基于 C++17、Qt 6 Widgets、`QOpenGLWidget`、FFmpeg 和 CMake 构建。

当前项目已经具备视频加载/解码、基础 H.264 语法解析、可停靠分析界面，以及基于 OpenGL 的视频渲染和分析叠加层。

## 功能概览

- 通过 `File -> Open` 或拖放打开本地 H.264/视频文件。
- 使用 FFmpeg 在后台线程中解码视频流，避免阻塞 UI。
- 使用 `QOpenGLWidget` 渲染解码后的视频帧。
- 使用自研 `H264Parser` 解析 H.264 NALU、SPS、PPS 和 Slice Header。
- 在右侧属性树中显示帧级语法信息。
- 在左侧帧列表中显示帧类型、POC 和 `frame_num`。
- 在视频画布上叠加分析数据：
  - 16x16 宏块网格
  - QP 热力图
  - 运动矢量绘制管线
- 支持生成包含 Qt、FFmpeg 和运行时 DLL 的 Windows 绿色发布包。

当前限制：

- 宏块级解析还未完成。
- QP 热力图目前使用 slice 级估算 QP，后续需要替换为真实宏块 QP。
- 运动矢量绘制接口已经接入，但真实 MV 仍需要继续从 `slice_data` 中解析。

## 项目结构

```text
h264_analyzer/
+-- docs/
+-- scripts/
+-- src/
|   +-- app/
|   +-- core/
|   +-- ui/
+-- CMakeLists.txt
+-- README.md
+-- README.zh-CN.md
+-- vcpkg.json
```

目录职责：

- `src/app`：应用窗口、菜单、工具栏、Dock 布局、文件打开和流程编排。
- `src/core`：码流文档模型、FFmpeg 解码封装、解码 Worker 和 H.264 语法解析器。
- `src/ui`：可复用 Qt 控件，例如帧列表、属性树、日志面板和视频画布。
- `scripts`：构建和发布辅助脚本。
- `docs`：开发说明、部署说明和 AI 后续开发路线图。

## 环境要求

- CMake 3.21 或更高版本。
- Qt 6，并包含 `Core`、`Widgets`、`OpenGL`、`OpenGLWidgets`。
- FFmpeg 开发库：
  - `avformat`
  - `avcodec`
  - `avutil`
  - `swscale`
- C++17 编译器：
  - Windows：MSVC 2022、MinGW-w64 或 MSYS2 UCRT64。
  - macOS：Apple Clang / Xcode Command Line Tools。
  - Linux：GCC 或 Clang。

官方参考：

- Qt 安装：https://doc.qt.io/qt-6/get-and-install-qt.html
- Qt CMake：https://doc.qt.io/qt-6/cmake-get-started.html
- CMake 下载：https://cmake.org/download/
- FFmpeg API 文档：https://ffmpeg.org/doxygen/trunk/
- vcpkg FFmpeg 包：https://vcpkg.io/en/package/ffmpeg.html

## Windows 开发环境

当前已经验证通过的 Windows 开发环境为 MSYS2 UCRT64：

- GCC/G++ 16.1.0
- CMake 4.3.2
- Ninja 1.13.2
- Qt 6.11.0
- FFmpeg 8.1.1

安装过程和验证结果见 [installEnv.md](installEnv.md)。

配置并编译：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake --build build-msys2-ucrt"
```

从开发环境运行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && ./build-msys2-ucrt/H264Analyzer.exe"
```

不要单独分发 `build-msys2-ucrt/H264Analyzer.exe`。它依赖 MSYS2 UCRT64 环境中的 DLL。

## Windows 绿色发布包

生成绿色发布包：

```powershell
.\scripts\deploy-windows-msys2.ps1
```

输出：

```text
dist/H264Analyzer-windows-ucrt64/
dist/H264Analyzer-windows-ucrt64.zip
```

绿色发布包包含：

- `H264Analyzer.exe`
- Qt 运行时 DLL
- Qt 插件，例如 `platforms/qwindows.dll`
- FFmpeg DLL，例如 `avcodec-62.dll`
- MSYS2 UCRT64/GCC 运行时 DLL

普通用户只需要解压并运行 `H264Analyzer.exe`。

更多说明见 [docs/windows-deployment.md](docs/windows-deployment.md)。

## macOS

安装依赖：

```bash
xcode-select --install
brew install cmake qt ffmpeg pkg-config
```

如果 CMake 找不到 Qt：

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

编译：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行：

```bash
open build/H264Analyzer.app
```

## Linux

Ubuntu / Debian：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Fedora：

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel mesa-libGL-devel ffmpeg-devel
```

编译：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

运行：

```bash
./build/H264Analyzer
```

## 开发说明

关键模块：

- `FFmpegDecoder`：封装 `AVFormatContext`、`AVCodecContext`、`AVPacket` 和 `AVFrame`。
- `DecodeWorker`：在后台 `QThread` 中执行解码。
- `H264Parser`：直接解析 H.264 语法，不依赖 FFmpeg parser。
- `VideoCanvas`：渲染视频帧和分析叠加层。
- `FrameListView` / `PropertyTreeView`：显示帧信息和语法树。

建议后续开发方向：

1. 增加播放控制和逐帧步进。
2. 补全 SPS/PPS/Slice Header 解析。
3. 实现真实宏块级 `mb_type` 和 QP 解析。
4. 从 `slice_data` 中解析真实运动矢量。
5. 增加宏块网格、QP 热力图、MV、透明度等叠加层控制。
6. 增加 JSON/CSV/截图导出。
7. 增加 CI，自动构建并发布 Windows 绿色包。

阶段化 AI 后续开发计划见 [docs/ai-next-steps.md](docs/ai-next-steps.md)。

## 仓库维护

不要提交构建或发布产物：

```text
build*/
dist/
```

这些路径已在 `.gitignore` 中忽略。
