# H.264 Analyzer

English | [简体中文](README.zh-CN.md)

H.264 Analyzer is a cross-platform desktop application framework for inspecting H.264 bitstreams. It is built with C++17, Qt 6 Widgets, `QOpenGLWidget`, FFmpeg, and CMake.

The project currently supports video loading/decoding, basic H.264 syntax parsing, a dockable analyzer UI, and OpenGL-based video rendering with analysis overlays.

## Features

- Open local H.264/video files through `File -> Open` or drag and drop.
- Decode video streams in a background thread with FFmpeg.
- Render decoded frames through `QOpenGLWidget`.
- Parse H.264 NALU, SPS, PPS, and Slice Header data with the custom `H264Parser`.
- Show frame syntax information in a dockable property tree.
- Show frame list information such as frame type, POC, and `frame_num`.
- Overlay analysis data on the video canvas:
  - 16x16 macroblock grid
  - QP heatmap
  - motion vector drawing pipeline
- Generate a Windows portable package that includes Qt, FFmpeg, and runtime DLLs.

Current limitations:

- Macroblock-level parsing is still incomplete.
- QP heatmap currently uses slice-level estimated QP until real macroblock QP parsing is implemented.
- Motion vector rendering is wired, but real MV extraction from `slice_data` still needs to be implemented.

## Project Structure

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

Folder responsibilities:

- `src/app`: application window, menu, toolbar, dock layout, file opening, and workflow wiring.
- `src/core`: stream document model, FFmpeg decoder wrapper, decode worker, and H.264 syntax parser.
- `src/ui`: reusable Qt widgets such as frame list, property tree, log dock, and video canvas.
- `scripts`: build/deployment helper scripts.
- `docs`: developer notes, deployment notes, and AI continuation roadmap.

## Requirements

- CMake 3.21 or later.
- Qt 6 with `Core`, `Widgets`, `OpenGL`, and `OpenGLWidgets`.
- FFmpeg development libraries:
  - `avformat`
  - `avcodec`
  - `avutil`
  - `swscale`
- C++17 compiler:
  - Windows: MSVC 2022, MinGW-w64, or MSYS2 UCRT64.
  - macOS: Apple Clang / Xcode Command Line Tools.
  - Linux: GCC or Clang.

Official references:

- Qt installation: https://doc.qt.io/qt-6/get-and-install-qt.html
- Qt with CMake: https://doc.qt.io/qt-6/cmake-get-started.html
- CMake downloads: https://cmake.org/download/
- FFmpeg API documentation: https://ffmpeg.org/doxygen/trunk/
- vcpkg FFmpeg package: https://vcpkg.io/en/package/ffmpeg.html

## Windows Development Environment

The current verified Windows development setup uses MSYS2 UCRT64:

- GCC/G++ 16.1.0
- CMake 4.3.2
- Ninja 1.13.2
- Qt 6.11.0
- FFmpeg 8.1.1

See [installEnv.md](installEnv.md) for the installation log and verification results.

Configure and build:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && cmake --build build-msys2-ucrt"
```

Run from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/h264_analyzer && ./build-msys2-ucrt/H264Analyzer.exe"
```

Do not distribute `build-msys2-ucrt/H264Analyzer.exe` alone. It depends on DLLs from the MSYS2 UCRT64 environment.

## Windows Portable Package

Create a portable package:

```powershell
.\scripts\deploy-windows-msys2.ps1
```

Output:

```text
dist/H264Analyzer-windows-ucrt64/
dist/H264Analyzer-windows-ucrt64.zip
```

The portable package includes:

- `H264Analyzer.exe`
- Qt runtime DLLs
- Qt plugins, such as `platforms/qwindows.dll`
- FFmpeg DLLs, such as `avcodec-62.dll`
- MSYS2 UCRT64/GCC runtime DLLs

End users only need to unzip the package and run `H264Analyzer.exe`.

For more details, see [docs/windows-deployment.md](docs/windows-deployment.md).

## macOS

Install dependencies:

```bash
xcode-select --install
brew install cmake qt ffmpeg pkg-config
```

If CMake cannot find Qt:

```bash
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
```

Build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run:

```bash
open build/H264Analyzer.app
```

## Linux

Ubuntu / Debian:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config qt6-base-dev qt6-base-dev-tools libgl1-mesa-dev libavformat-dev libavcodec-dev libavutil-dev libswscale-dev
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel mesa-libGL-devel ffmpeg-devel
```

Build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Run:

```bash
./build/H264Analyzer
```

## Development Notes

Key modules:

- `FFmpegDecoder`: wraps `AVFormatContext`, `AVCodecContext`, `AVPacket`, and `AVFrame`.
- `DecodeWorker`: runs decoding on a background `QThread`.
- `H264Parser`: directly parses H.264 syntax without relying on FFmpeg's parser.
- `VideoCanvas`: renders video frames and analysis overlays.
- `FrameListView` and `PropertyTreeView`: display parsed frame and syntax information.

Recommended next work:

1. Add playback controls and frame stepping.
2. Complete SPS/PPS/Slice Header parsing.
3. Implement real macroblock-level `mb_type` and QP parsing.
4. Extract real motion vectors from `slice_data`.
5. Add overlay controls for grid, QP heatmap, MV, and opacity.
6. Add JSON/CSV/screenshot export.
7. Add CI to build and publish Windows portable packages.

For a staged AI continuation plan, see [docs/ai-next-steps.md](docs/ai-next-steps.md).

## Repository Hygiene

Do not commit generated build or release outputs:

```text
build*/
dist/
```

These paths are ignored by `.gitignore`.
