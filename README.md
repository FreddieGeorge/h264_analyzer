# ZStreamEye

[![Windows MSYS2](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml/badge.svg)](https://github.com/FreddieGeorge/ZStreamEye/actions/workflows/windows-msys2.yml)

English | [简体中文](README.zh-CN.md)

ZStreamEye is a cross-platform desktop application framework for inspecting H.264 bitstreams. It is built with C++17, Qt 6 Widgets, `QOpenGLWidget`, FFmpeg, and CMake.

The project currently supports video loading/decoding, controllable playback, H.264 syntax parsing, macroblock-level overlays, export tools, a dockable analyzer UI, and OpenGL-based video rendering.

## Features

- Open local H.264/video files through `File -> Open` or drag and drop.
- Decode video streams in a background thread with FFmpeg.
- Discover container streams, including basic video/audio metadata, while still
  decoding the best video stream by default.
- Play, pause, stop, and step frame-by-frame.
- Rebuffer evicted frames from indexed keyframe/IDR seek checkpoints when available.
- Render decoded frames through `QOpenGLWidget`.
- Route bitstream parsing through a codec-neutral parser interface so additional codecs can be added without coupling them to `FFmpegDecoder`.
- Provide an HEVC/H.265 parser skeleton that identifies NAL units, VPS/SPS/PPS,
  VCL access units, and graceful unsupported slice diagnostics.
- Provide an AAC ADTS parser skeleton for audio access units, including header
  bit fields and graceful diagnostics for malformed packets.
- Provide an MP3 frame-header parser skeleton for MPEG audio frame access units.
- List parsed audio access units from discovered AAC/MP3 audio streams alongside
  video frames, and show their generic fields in the property tree.
- Parse H.264 NALU, SPS, PPS, Slice Header, selected CAVLC macroblock fields, residual blocks, QP, and P-slice L0 motion vectors with the custom `H264Parser`.
- Show frame syntax information in a dockable property tree.
- Show overlay availability in the property tree, including QP range/constant
  notes and motion-vector support diagnostics for the current frame.
- Show frame list information such as frame type, POC, and `frame_num`.
- Overlay analysis data on the video canvas:
  - 16x16 macroblock grid
  - macroblock QP heatmap
  - parsed P-slice motion vectors
- Toggle grid/QP/MV overlays and adjust overlay opacity.
- Persist window layout, dock positions, overlay toggles, opacity, and recent open/export directories.
- Export selected access-unit syntax JSON, access-unit list CSV, screenshots
  with overlays, and all decoded access-unit syntax JSON with schema/version
  and stream metadata.
- Generate a Windows portable package that includes Qt, FFmpeg, and runtime DLLs.
- Check GitHub Releases for updates from `Help -> Check for Updates`.

Current limitations:

- CAVLC residual parsing consumes and counts residual blocks so macroblock parsing can continue, but individual coefficient values are not yet exposed in the UI/export model.
- CABAC, B_Direct/B_8x8 motion vectors, and MBAFF/FMO are reported as unsupported or partially parsed; CAVLC P_8x8/P_8x8ref0 L0 and non-direct B_L0/B_L1/Bi motion vectors have focused parser coverage.
- The property tree limits displayed macroblocks to keep playback responsive; overlay data still uses the parsed macroblock list.
- OpenGL canvas text is intentionally avoided for analysis hints because some
  Windows/OpenGL deployments render QPainter text incorrectly; use the property
  tree and status bar for analysis explanations.

## Project Structure

```text
ZStreamEye/
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
- `src/core`: stream document model, FFmpeg decoder wrapper, decode worker, codec-neutral parser interface, and H.264 syntax parser.
- `src/ui`: reusable Qt widgets such as frame list, property tree, log dock, and video canvas.
- `scripts`: build/deployment helper scripts.
- `docs`: developer notes, deployment notes, and AI continuation roadmap.

## Requirements

- CMake 3.21 or later.
- Qt 6 with `Core`, `Widgets`, `Network`, `OpenGL`, and `OpenGLWidgets`.
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
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
```

Run from the development environment:

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

Do not distribute `build-msys2-ucrt/ZStreamEye.exe` alone. It depends on DLLs from the MSYS2 UCRT64 environment.

## Windows Portable Package

Create a portable package:

```powershell
.\scripts\deploy-windows-msys2.ps1
```

Output:

```text
dist/ZStreamEye-windows-ucrt64/
dist/ZStreamEye-windows-ucrt64.zip
```

The portable package includes:

- `ZStreamEye.exe`
- Qt runtime DLLs
- Qt plugins, such as `platforms/qwindows.dll`
- FFmpeg DLLs, such as `avcodec-62.dll`
- MSYS2 UCRT64/GCC runtime DLLs

End users only need to unzip the package and run `ZStreamEye.exe`. Runtime DLLs
and Qt plugins are grouped under `runtime/`.

## Windows Installer

Create a Windows installer after the portable package has been generated:

```powershell
.\scripts\deploy-windows-msys2.ps1
.\scripts\package-windows-installer.ps1 -Version "0.1.8"
```

Output:

```text
dist/ZStreamEye-0.1.8-windows-ucrt64-setup.exe
```

The installer is built with Inno Setup 6 and installs the same self-contained
Qt, FFmpeg, and MSYS2 UCRT64 runtime files as the portable package. GitHub
release builds publish both the portable `.zip` and the installer `.exe`.
Tag releases as `vX.Y.Z`; the release workflow verifies that the tag matches
the CMake project version before publishing GitHub Release assets.

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

Run tests:

```bash
ctest --test-dir build --output-on-failure
```

Run:

```bash
open build/ZStreamEye.app
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
./build/ZStreamEye
```

## Development Notes

Key modules:

- `FFmpegDecoder`: wraps `AVFormatContext`, `AVCodecContext`, `AVPacket`, and `AVFrame`; owns codec-specific bitstream parsers through `IBitstreamParser`.
- `MediaTypes`: media/access-unit identifiers used to keep future video and
  audio analysis paths from being forced into video-only fields.
- `BitstreamParser`: codec-neutral parser interface and codec kind identifiers.
- `AacAdtsParser`: thin audio access-unit skeleton for ADTS header fields and
  diagnostics.
- `Mp3FrameParser`: thin audio access-unit skeleton for MPEG audio frame
  headers, bitrate/sample-rate fields, and malformed-header diagnostics.
- `DecodeWorker`: runs decoding on a background `QThread`.
- `H264Parser`: directly parses H.264 syntax without relying on FFmpeg's parser.
- `VideoCanvas`: renders video frames and analysis overlays.
- `FrameListView` and `PropertyTreeView`: display parsed frame and syntax information.

Recommended next work:

1. Smoke-test the latest Windows installer/portable release and upgrade path.
2. Broaden old-frame rebuffer smoke coverage with real raw Annex B `.264` and
   indexed `.mp4`/`.mkv` files; cancellation/progress and core state tests are
   already in place.
3. Expand parser coverage beyond the focused CAVLC P_8x8/P_8x8ref0 and
   non-direct B-slice fixtures; keep B_Direct/B_8x8/CABAC diagnostics precise.
4. Add explicit stream selection and richer packet/access-unit browsing on top
   of the AAC/MP3 skeletons; keep audio analysis separate from `VideoCanvas`.
5. Expose richer residual coefficient details and broaden macroblock support.
6. Add a bitstream hex dock linked to existing syntax field bit offsets.

For future AI/coding agents, see [docs/ai-continuation-notes.md](docs/ai-continuation-notes.md).
For the longer-term StreamEye-class roadmap, see [docs/ai-streameye-roadmap.md](docs/ai-streameye-roadmap.md).

## Repository Hygiene

Do not commit generated build or release outputs:

```text
build*/
dist/
```

These paths are ignored by `.gitignore`.
