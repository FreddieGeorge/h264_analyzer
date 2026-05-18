# H.264 Analyzer

[English](README.md) | 简体中文

H.264 Analyzer 是一个跨平台桌面 H.264 码流分析工具，基于 C++17、Qt 6 Widgets、`QOpenGLWidget`、FFmpeg 和 CMake 构建。

## 功能概览

- 通过 `File -> Open` 或拖放打开本地 H.264/视频文件。
- 使用 FFmpeg 在后台线程中解码，避免阻塞 UI。
- 支持播放、暂停、停止、上一帧和下一帧。
- 使用自研 `H264Parser` 解析 NALU、SPS、PPS、Slice Header、部分 CAVLC 宏块字段、QP 和 P-slice L0 运动矢量。
- 左侧显示帧列表，右侧显示可展开的语法属性树。
- 视频画布支持分析 overlay：
  - 16x16 宏块网格
  - 宏块 QP 热力图
  - P-slice 运动矢量
- 支持独立开关 grid、QP、MV overlay，并调整 overlay 透明度。
- 支持导出当前帧语法 JSON、帧列表 CSV 和带 overlay 的截图。
- 支持生成包含 Qt、FFmpeg、MSYS2 UCRT64 运行时 DLL 的 Windows 便携包。

当前限制：

- 残差系数解析仍不完整。
- CABAC、B-slice 运动矢量、MBAFF/FMO、sub-macroblock MV 暂时只做安全跳过或部分解析。
- 为保持 UI 响应，属性树只展示前若干宏块；overlay 仍使用解析得到的宏块数据。

## Windows 开发环境

当前验证环境为 MSYS2 UCRT64：

- GCC/G++ 16.1.0
- CMake 4.3.2
- Ninja 1.13.2
- Qt 6.11.0
- FFmpeg 8.1.1

配置并构建：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake --build build-msys2-ucrt"
```

运行测试：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && ctest --test-dir build-msys2-ucrt --output-on-failure"
```

从开发环境运行：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && ./build-msys2-ucrt/H264Analyzer.exe"
```

不要单独分发 `build-msys2-ucrt/H264Analyzer.exe`，它依赖 MSYS2 UCRT64 环境中的 DLL。

## Windows 便携包

生成便携包：

```powershell
.\scripts\deploy-windows-msys2.ps1
```

如果当前 PowerShell 禁止执行脚本，可以只对当前进程绕过策略：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\deploy-windows-msys2.ps1
```

输出：

```text
dist/H264Analyzer-windows-ucrt64/
dist/H264Analyzer-windows-ucrt64.zip
```

终端用户解压后运行 `H264Analyzer.exe` 即可。

## Linux / macOS

安装 Qt 6、FFmpeg、CMake、Ninja 和 C++17 编译器后可使用常规 CMake 流程：

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## 项目结构

```text
h264_analyzer/
+-- docs/
+-- scripts/
+-- src/
|   +-- app/
|   +-- core/
|   +-- ui/
+-- tests/
+-- CMakeLists.txt
```

关键模块：

- `FFmpegDecoder`：封装 FFmpeg 解码。
- `DecodeWorker`：在后台 `QThread` 中执行解码。
- `H264Parser`：直接解析 H.264 语法，不依赖 FFmpeg parser。
- `VideoCanvas`：渲染视频帧和分析 overlay。
- `FrameListView` / `PropertyTreeView`：显示帧列表和语法树。

## CI

仓库包含 GitHub Actions Windows MSYS2 workflow，会执行配置、构建、测试、生成便携 zip，并上传构建产物。

## 仓库维护

不要提交生成物：

```text
build*/
dist/
```
