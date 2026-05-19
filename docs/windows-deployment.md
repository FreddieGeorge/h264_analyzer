# Windows Portable Deployment

普通用户不需要安装 MSYS2、Qt 或 FFmpeg。发布时使用脚本生成绿色运行包：

```powershell
.\scripts\deploy-windows-msys2.ps1
```

输出：

- `dist/H264Analyzer-windows-ucrt64/`
- `dist/H264Analyzer-windows-ucrt64.zip`

把 zip 发给用户后，用户解压并运行 `H264Analyzer.exe` 即可。发布目录会包含：

- 主程序 `H264Analyzer.exe`
- Qt 运行时 DLL
- Qt plugins，例如 `platforms/qwindows.dll`
- FFmpeg DLL，例如 `avcodec-62.dll`
- MSYS2 UCRT64/GCC 运行时 DLL

不要把 `build-msys2-ucrt/H264Analyzer.exe` 单独发给用户；它依赖本机 `C:\msys64\ucrt64\bin` 中的 DLL。

## Windows Installer

The installer uses the same portable folder as its source. Install Inno Setup 6
locally, then run:

```powershell
.\scripts\deploy-windows-msys2.ps1
.\scripts\package-windows-installer.ps1 -Version "0.1.0"
```

Output:

- `dist/H264Analyzer-0.1.0-windows-ucrt64-setup.exe`

The GitHub release workflow installs Inno Setup and uploads both:

- `H264Analyzer-<version>-windows-ucrt64.zip`
- `H264Analyzer-<version>-windows-ucrt64-setup.exe`
