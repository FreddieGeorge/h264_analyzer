# Windows Portable Deployment

普通用户不需要安装 MSYS2、Qt 或 FFmpeg。发布时使用脚本生成绿色运行包：

```powershell
.\scripts\deploy-windows-msys2.ps1
```

The deployment script owns a dedicated CMake build directory:

```text
build-deploy-msys2-ucrt/
```

It configures this directory before building the portable package, so it does
not depend on a developer/debug build directory such as `build-msys2-ucrt/`.

输出：

- `dist/ZStreamEye-windows-ucrt64/`
- `dist/ZStreamEye-windows-ucrt64.zip`

把 zip 发给用户后，用户解压并运行 `ZStreamEye.exe` 即可。发布目录会包含：

- 主程序 `ZStreamEye.exe`
- Qt 运行时 DLL
- Qt plugins，例如 `platforms/qwindows.dll`
- FFmpeg DLL，例如 `avcodec-62.dll`
- MSYS2 UCRT64/GCC 运行时 DLL

不要把任何 build 目录中的 `ZStreamEye.exe` 单独发给用户；它依赖本机 `C:\msys64\ucrt64\bin` 中的 DLL。发布包会把运行时 DLL 和插件统一放在 `runtime\` 目录下。

## Windows Installer

The installer uses the same portable folder as its source. Install Inno Setup 6
locally, then run:

```powershell
.\scripts\deploy-windows-msys2.ps1
.\scripts\package-windows-installer.ps1 -Version "0.1.8"
```

Output:

- `dist/ZStreamEye-0.1.8-windows-ucrt64-setup.exe`

The GitHub release workflow installs Inno Setup and uploads both:

- `ZStreamEye-<version>-windows-ucrt64.zip`
- `ZStreamEye-<version>-windows-ucrt64-setup.exe`
