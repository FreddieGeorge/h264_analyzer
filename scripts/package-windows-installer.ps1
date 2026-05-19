param(
    [string]$DistDir = "dist/ZStreamEye-windows-ucrt64",
    [string]$OutputDir = "dist",
    [string]$Version = "0.1.6",
    [string]$InstallerScript = "installer/ZStreamEye.iss",
    [string]$ISCCPath = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$distPath = Join-Path $repoRoot $DistDir
$outputPath = Join-Path $repoRoot $OutputDir
$scriptPath = Join-Path $repoRoot $InstallerScript

function Assert-PathUnderRepo {
    param([string]$Path)
    $full = [System.IO.Path]::GetFullPath($Path)
    $root = [System.IO.Path]::GetFullPath($repoRoot)
    if (-not $full.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to use path outside repository: $full"
    }
}

function Resolve-ISCC {
    param([string]$ExplicitPath)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $candidates += $ExplicitPath
    }
    if (-not [string]::IsNullOrWhiteSpace($env:ISCC)) {
        $candidates += $env:ISCC
    }
    $command = Get-Command "iscc.exe" -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        $candidates += $command.Source
    }
    $candidates += @(
        (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
        (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe")
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Inno Setup compiler not found. Install Inno Setup 6 or pass -ISCCPath."
}

Assert-PathUnderRepo $distPath
Assert-PathUnderRepo $outputPath
Assert-PathUnderRepo $scriptPath

if (-not (Test-Path (Join-Path $distPath "bin\ZStreamEye.exe"))) {
    throw "Portable package is missing bin\ZStreamEye.exe: $distPath"
}
if (-not (Test-Path $scriptPath)) {
    throw "Installer script not found: $scriptPath"
}

New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

$iscc = Resolve-ISCC $ISCCPath
$safeVersion = $Version -replace '[^A-Za-z0-9_.-]', '-'
$outputBaseFilename = "ZStreamEye-$safeVersion-windows-ucrt64-setup"
$installerPath = Join-Path $outputPath "$outputBaseFilename.exe"

if (Test-Path $installerPath) {
    Remove-Item -LiteralPath $installerPath -Force
}

Write-Host "Using Inno Setup compiler: $iscc"
Write-Host "Creating Windows installer: $installerPath"

& $iscc `
    "/DAppVersion=$Version" `
    "/DSourceDir=$distPath" `
    "/DOutputDir=$outputPath" `
    "/DOutputBaseFilename=$outputBaseFilename" `
    $scriptPath

if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compiler failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path $installerPath)) {
    throw "Installer was not created: $installerPath"
}

Write-Host "Installer ready: $installerPath"
