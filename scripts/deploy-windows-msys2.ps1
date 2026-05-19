param(
    [string]$BuildDir = "build-msys2-ucrt",
    [string]$DistDir = "dist/ZStreamEye-windows-ucrt64",
    [string]$MsysRoot = "",
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
$distPath = Join-Path $repoRoot $DistDir
$runtimePath = Join-Path $distPath "runtime"

function Resolve-MsysRoot {
    param([string]$ExplicitRoot)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitRoot)) {
        $candidates += $ExplicitRoot
    }
    if (-not [string]::IsNullOrWhiteSpace($env:MSYS2_LOCATION)) {
        $candidates += $env:MSYS2_LOCATION
    }
    $candidates += "C:\msys64"

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        $bashCandidate = Join-Path $candidate "usr\bin\bash.exe"
        if (Test-Path $bashCandidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "MSYS2 root not found. Tried: $($candidates -join ', ')"
}

$msysRoot = Resolve-MsysRoot $MsysRoot
$ucrtBin = Join-Path $msysRoot "ucrt64\bin"
$bash = Join-Path $msysRoot "usr\bin\bash.exe"
$objdump = Join-Path $ucrtBin "objdump.exe"

function Assert-ToolExists {
    param([string]$Path)
    if (-not (Test-Path $Path)) {
        throw "Required tool not found: $Path"
    }
}

function Resolve-FirstExistingTool {
    param(
        [string[]]$Paths,
        [string]$CandidatePattern = "*"
    )
    foreach ($path in $Paths) {
        if (Test-Path $path) {
            return $path
        }
    }

    $candidateDirs = $Paths | ForEach-Object { Split-Path -Parent $_ } | Sort-Object -Unique
    foreach ($candidateDir in $candidateDirs) {
        Write-Host "Available ${CandidatePattern} candidates under ${candidateDir}:"
        $candidates = Get-ChildItem -Path $candidateDir -Filter $CandidatePattern -ErrorAction SilentlyContinue
        if ($candidates) {
            $candidates | ForEach-Object { Write-Host "  $($_.FullName)" }
        } else {
            Write-Host "  none"
        }
    }

    if (Test-Path $bash) {
        Write-Host "Installed Qt deployment-related MSYS2 packages:"
        & $bash -lc "pacman -Q mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-tools 2>/dev/null || true"
    }

    throw "Required tool not found. Tried: $($Paths -join ', ')"
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
$windeployqt = Resolve-FirstExistingTool -Paths @(
    (Join-Path $ucrtBin "windeployqt6.exe"),
    (Join-Path $ucrtBin "windeployqt.exe")
) -CandidatePattern "windeployqt*.exe"
Assert-ToolExists $objdump
Assert-PathUnderRepo $distPath

$repoRootMsys = (& $bash -lc "cygpath -u '$repoRoot'").Trim()
$buildPathMsys = (& $bash -lc "cygpath -u '$buildPath'").Trim()
$env:PATH = "$ucrtBin;$env:PATH"
Write-Host "Using MSYS2 root: $msysRoot"
Write-Host "Using windeployqt: $windeployqt"

Write-Host "Building project..."
& $bash -lc "export PATH=/ucrt64/bin:/usr/bin:`$PATH; cd '$repoRootMsys' && cmake --build '$buildPathMsys'"

$launcherExe = Join-Path $buildPath "ZStreamEye.exe"
$appExe = Join-Path $buildPath "ZStreamEyeApp.exe"
if (-not (Test-Path $launcherExe)) {
    throw "Executable not found: $launcherExe"
}
if (-not (Test-Path $appExe)) {
    throw "Executable not found: $appExe"
}

if (Test-Path $distPath) {
    $fullDist = [System.IO.Path]::GetFullPath($distPath)
    $fullRoot = [System.IO.Path]::GetFullPath($repoRoot)
    if (-not $fullDist.StartsWith($fullRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove path outside repository: $fullDist"
    }
    Remove-Item -LiteralPath $distPath -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $runtimePath | Out-Null
Copy-Item -LiteralPath $launcherExe -Destination (Join-Path $distPath "ZStreamEye.exe")
Copy-Item -LiteralPath $appExe -Destination (Join-Path $runtimePath "ZStreamEyeApp.exe")

Write-Host "Deploying Qt runtime and plugins..."
& $windeployqt `
    --dir $runtimePath `
    --compiler-runtime `
    --no-translations `
    --no-system-d3d-compiler `
    --no-opengl-sw `
    (Join-Path $runtimePath "ZStreamEyeApp.exe")

Write-Host "Collecting FFmpeg/MSYS2 runtime DLL closure..."
Copy-MsysRuntimeClosure -TargetDir $runtimePath

$readmePath = Join-Path $distPath "README-RUN.txt"
@"
ZStreamEye Windows portable package

Run:
  ZStreamEye.exe

The runtime folder contains the Qt, FFmpeg and MSYS2 UCRT64 runtime DLLs required by the application.
No MSYS2, Qt or FFmpeg installation is required on the target machine.
"@ | Set-Content -Path $readmePath -Encoding ASCII

if (-not $NoZip) {
    $zipPath = Join-Path $repoRoot "dist/ZStreamEye-windows-ucrt64.zip"
    Assert-PathUnderRepo $zipPath
    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $distPath "*") -DestinationPath $zipPath -Force
    Write-Host "Created ZIP: $zipPath"
}

Write-Host "Portable package ready: $distPath"
