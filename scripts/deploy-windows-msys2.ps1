param(
    [string]$BuildDir = "build-msys2-ucrt",
    [string]$DistDir = "dist/H264Analyzer-windows-ucrt64",
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
$distPath = Join-Path $repoRoot $DistDir
$msysRoot = "C:\msys64"
$ucrtBin = Join-Path $msysRoot "ucrt64\bin"
$bash = Join-Path $msysRoot "usr\bin\bash.exe"
$windeployqt = Join-Path $ucrtBin "windeployqt6.exe"
$objdump = Join-Path $ucrtBin "objdump.exe"
$repoRootMsys = (& $bash -lc "cygpath -u '$repoRoot'").Trim()
$buildPathMsys = (& $bash -lc "cygpath -u '$buildPath'").Trim()

function Assert-ToolExists {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Required tool not found: $Path"
    }
}

function Assert-PathUnderRepo {
    param([string]$Path)
    $resolvedParent = Resolve-Path (Split-Path -Parent $Path) -ErrorAction SilentlyContinue
    if ($null -eq $resolvedParent) {
        $resolvedParent = Resolve-Path $repoRoot
    }
    $full = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetFullPath($repoRoot)
    if (-not $full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to write outside repository: $full"
    }
}

function Get-ImportedDllNames {
    param([string]$BinaryPath)

    & $objdump -p $BinaryPath 2>$null |
        Select-String "DLL Name: (.+)$" |
        ForEach-Object { $_.Matches[0].Groups[1].Value.Trim() }
}

function Copy-MsysRuntimeClosure {
    param([string]$TargetDir)

    $copied = $true
    while ($copied) {
        $copied = $false
        $binaries = Get-ChildItem -Path $TargetDir -Recurse -File |
            Where-Object { $_.Extension -ieq ".exe" -or $_.Extension -ieq ".dll" }

        foreach ($binary in $binaries) {
            foreach ($dllName in Get-ImportedDllNames $binary.FullName) {
                $source = Join-Path $ucrtBin $dllName
                $dest = Join-Path $TargetDir $dllName
                if ((Test-Path $source) -and -not (Test-Path $dest)) {
                    Copy-Item -LiteralPath $source -Destination $dest
                    Write-Host "Copied runtime DLL: $dllName"
                    $copied = $true
                }
            }
        }
    }
}

Assert-ToolExists $bash
Assert-ToolExists $windeployqt
Assert-ToolExists $objdump
Assert-PathUnderRepo $distPath

$env:PATH = "$ucrtBin;$env:PATH"

Write-Host "Building project..."
& $bash -lc "export PATH=/ucrt64/bin:/usr/bin:`$PATH; cd '$repoRootMsys' && cmake --build '$buildPathMsys'"

$sourceExe = Join-Path $buildPath "H264Analyzer.exe"
if (-not (Test-Path $sourceExe)) {
    throw "Executable not found: $sourceExe"
}

if (Test-Path $distPath) {
    $fullDist = [System.IO.Path]::GetFullPath($distPath)
    $fullRoot = [System.IO.Path]::GetFullPath($repoRoot)
    if (-not $fullDist.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside repository: $fullDist"
    }
    Remove-Item -LiteralPath $distPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $distPath | Out-Null
Copy-Item -LiteralPath $sourceExe -Destination (Join-Path $distPath "H264Analyzer.exe")

Write-Host "Deploying Qt runtime and plugins..."
& $windeployqt `
    --dir $distPath `
    --compiler-runtime `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    (Join-Path $distPath "H264Analyzer.exe")

Write-Host "Collecting FFmpeg/MSYS2 runtime DLL closure..."
Copy-MsysRuntimeClosure -TargetDir $distPath

$readmePath = Join-Path $distPath "README-RUN.txt"
@"
H264Analyzer Windows portable package

Run:
  H264Analyzer.exe

This folder contains the Qt, FFmpeg and MSYS2 UCRT64 runtime DLLs required by the application.
No MSYS2, Qt or FFmpeg installation is required on the target machine.
"@ | Set-Content -Path $readmePath -Encoding ASCII

if (-not $NoZip) {
    $zipPath = Join-Path $repoRoot "dist/H264Analyzer-windows-ucrt64.zip"
    Assert-PathUnderRepo $zipPath
    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $distPath "*") -DestinationPath $zipPath -Force
    Write-Host "Created ZIP: $zipPath"
}

Write-Host "Portable package ready: $distPath"
