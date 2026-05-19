# ZStreamEye 开发环境安装记录

## 当前机器

- 工作目录：`D:\Desktop\ZStreamEye`
- 系统：Windows 10 x64
- Shell：PowerShell

## 初始检查

- 已发现：`winget`、Git、Python launcher、`D:\MinGW\bin\g++.exe`
- 未发现：`cmake`、`ninja`、Qt 工具、`vcpkg`

### 初始检查输出

```text
winget: C:\Users\Administrator\AppData\Local\Microsoft\WindowsApps\winget.exe
git: D:\Software\Git\cmd\git.exe
python launcher: C:\Windows\py.exe
g++: D:\MinGW\bin\g++.exe

py --version:
Can't find a default Python.

git --version:
git version 2.38.0.windows.1

g++ --version:
g++.exe (MinGW.org GCC-6.3.0-1) 6.3.0
```

结论：当前已有 MinGW 版本过旧，不建议用于 Qt6/C++17。后续采用 MSYS2 UCRT64 工具链安装预编译 Qt6/FFmpeg/CMake/Ninja。

## 安装计划

1. 安装 CMake。
2. 安装 Ninja。
3. 安装或准备 vcpkg，并安装 FFmpeg 依赖。
4. 安装 Qt 6 开发库。
5. 验证 CMake/Qt/FFmpeg 是否能被当前项目发现。

## 安装过程

### 1. 安装 MSYS2

执行命令：

```powershell
winget install --id MSYS2.MSYS2 --source winget --accept-package-agreements --accept-source-agreements --silent
```

结果：

```text
已找到 MSYS2 Installer [MSYS2.MSYS2] 版本 20260322
下载地址：https://github.com/msys2/msys2-installer/releases/download/2026-03-22/msys2-x86_64-20260322.exe
已成功验证安装程序哈希
已成功安装
```

安装目录检查：

```text
C:\msys64
C:\msys64\usr\bin\bash.exe: True
C:\msys64\usr\bin\pacman.exe: True
```

说明：本机原有 MinGW 版本较旧，因此后续统一使用 MSYS2 的 UCRT64 工具链。

### 2. 更新 MSYS2 并安装开发包

执行命令：

```powershell
C:\msys64\usr\bin\bash.exe -lc "pacman --noconfirm -Syuu"
C:\msys64\usr\bin\bash.exe -lc "pacman --noconfirm -Syuu"
C:\msys64\usr\bin\bash.exe -lc "pacman --noconfirm -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-ffmpeg mingw-w64-ucrt-x86_64-pkgconf"
```

安装包：

- `mingw-w64-ucrt-x86_64-toolchain`
- `mingw-w64-ucrt-x86_64-cmake`
- `mingw-w64-ucrt-x86_64-ninja`
- `mingw-w64-ucrt-x86_64-qt6-base`
- `mingw-w64-ucrt-x86_64-ffmpeg`
- `mingw-w64-ucrt-x86_64-pkgconf`

安装过程中出现过一条 MSYS2 镜像 404 提示，但最终 `pacman -Q` 和版本检查均确认目标包已安装成功。

### 3. 工具链版本验证

执行命令：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; gcc --version | head -1; g++ --version | head -1; cmake --version | head -1; ninja --version; qmake6 -v; qtpaths6 --version; pkg-config --modversion libavformat libavcodec libavutil libswscale"
```

结果：

```text
gcc.exe (Rev5, Built by MSYS2 project) 16.1.0
g++.exe (Rev5, Built by MSYS2 project) 16.1.0
cmake version 4.3.2
Ninja 1.13.2
QMake version 3.1
Using Qt version 6.11.0 in C:/msys64/ucrt64/lib
qtpaths6 2.0
libavformat 62.12.101
libavcodec 62.28.101
libavutil 60.26.101
libswscale 9.5.101
```

`pacman -Q` 结果：

```text
mingw-w64-ucrt-x86_64-gcc 16.1.0-5
mingw-w64-ucrt-x86_64-cmake 4.3.2-4
mingw-w64-ucrt-x86_64-ninja 1.13.2-1
mingw-w64-ucrt-x86_64-qt6-base 6.11.0-6
mingw-w64-ucrt-x86_64-ffmpeg 8.1.1-1
mingw-w64-ucrt-x86_64-pkgconf 1~2.5.1-1
```

## 项目配置与编译验证

### 1. CMake 配置

执行命令：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
```

结果：

```text
-- The CXX compiler identification is GNU 16.1.0
-- Found Threads: TRUE
-- Found WrapAtomic: TRUE
-- Found WrapVulkanHeaders: C:/msys64/ucrt64/include
-- Configuring done
-- Generating done
-- Build files have been written to: D:/Desktop/ZStreamEye/build-msys2-ucrt
```

结论：CMake 能找到 Qt6、FFmpeg、GCC、Ninja。

### 2. 编译

执行命令：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
```

结果：

```text
[1/4] Automatic MOC and UIC for target ZStreamEye
[2/3] Building CXX object CMakeFiles/ZStreamEye.dir/src/app/MainWindow.cpp.obj
[3/3] Linking CXX executable ZStreamEye.exe
```

结论：项目编译成功。

### 3. 可执行文件与依赖检查

执行命令：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ls -lh build-msys2-ucrt/ZStreamEye.exe"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ldd build-msys2-ucrt/ZStreamEye.exe | grep 'not found' || true"
```

结果：

```text
-rwxr-xr-x 1 Administrator None 9.7M 5月 18 21:52 build-msys2-ucrt/ZStreamEye.exe
未发现 not found 依赖项。
```

## 后续使用方式

在 PowerShell 中配置和编译：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64"
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && cmake --build build-msys2-ucrt"
```

运行程序：

```powershell
C:\msys64\usr\bin\bash.exe -lc "export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Desktop/ZStreamEye && ./build-msys2-ucrt/ZStreamEye.exe"
```

也可以打开“MSYS2 UCRT64”终端后执行：

```bash
cd /d/Desktop/ZStreamEye
cmake -S . -B build-msys2-ucrt -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/ucrt64
cmake --build build-msys2-ucrt
./build-msys2-ucrt/ZStreamEye.exe
```

## 最终状态

- MSYS2 UCRT64：已安装
- GCC/G++：已安装，版本 16.1.0
- CMake：已安装，版本 4.3.2
- Ninja：已安装，版本 1.13.2
- Qt6 Base：已安装，版本 6.11.0
- FFmpeg：已安装，版本 8.1.1
- 项目 CMake 配置：通过
- 项目编译：通过
- 生成文件：`D:\Desktop\ZStreamEye\build-msys2-ucrt\ZStreamEye.exe`
## Windows Portable Package

Do not distribute `build-msys2-ucrt\ZStreamEye.exe` alone. That executable depends on DLLs from `C:\msys64\ucrt64\bin`, such as `avcodec-62.dll`.

For end users, create a portable package:

```powershell
.\scripts\deploy-windows-msys2.ps1
```

The script performs these steps:

1. Builds the project.
2. Creates `dist\ZStreamEye-windows-ucrt64\`.
3. Copies `ZStreamEye.exe`.
4. Runs `windeployqt6` to collect Qt DLLs and plugins.
5. Recursively scans EXE/DLL imports and copies FFmpeg/MSYS2 UCRT64 runtime DLLs from `C:\msys64\ucrt64\bin`.
6. Creates `dist\ZStreamEye-windows-ucrt64.zip`.

Result on this machine:

```text
Portable package ready: D:\Desktop\ZStreamEye\dist\ZStreamEye-windows-ucrt64
Created ZIP: D:\Desktop\ZStreamEye\dist\ZStreamEye-windows-ucrt64.zip
dist\ZStreamEye-windows-ucrt64\avcodec-62.dll exists
dist\ZStreamEye-windows-ucrt64.zip size: 88,234,032 bytes
Portable folder total size: 233,224,438 bytes, 123 files
Dependency check with clean PATH: no "not found" entries
Launch smoke test: Started successfully; stopping test process
```

Distribute this file:

```text
dist\ZStreamEye-windows-ucrt64.zip
```

The user only needs to unzip it and run:

```text
ZStreamEye.exe
```
