# PowerShell script to copy LEGO Indiana Jones 2 assets into the Android Project
$ErrorActionPreference = "Stop"

$SourceDir = "D:\Game\LEGO Indiana Jones 2 The Adventure Continues"
$TargetAssetsDir = Join-Path $PSScriptRoot "app\src\main\assets"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " LEGO Indiana Jones 2 - Android Asset Preparation Script " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if (-not (Test-Path $SourceDir)) {
    Write-Error "Source directory not found: $SourceDir"
    exit 1
}

Write-Host "[1/3] Ensuring target assets directory exists..."
if (-not (Test-Path $TargetAssetsDir)) {
    New-Item -ItemType Directory -Path $TargetAssetsDir -Force | Out-Null
}

Write-Host "[2/3] Copying primary game archive (legoindy2psp.fib)..."
$FibSource = Join-Path $SourceDir "PSP_GAME\USRDIR\legoindy2psp.fib"
if (Test-Path $FibSource) {
    Copy-Item -Path $FibSource -Destination $TargetAssetsDir -Force
    Write-Host " -> Copied legoindy2psp.fib successfully." -ForegroundColor Green
} else {
    Write-Warning " -> legoindy2psp.fib not found in $FibSource"
}

Write-Host "[3/3] Copying metadata and PARAM.SFO..."
$ParamSource = Join-Path $SourceDir "PSP_GAME\PARAM.SFO"
if (Test-Path $ParamSource) {
    Copy-Item -Path $ParamSource -Destination $TargetAssetsDir -Force
    Write-Host " -> Copied PARAM.SFO successfully." -ForegroundColor Green
}

Write-Host ""
Write-Host "Asset setup complete! Assets are now available at: $TargetAssetsDir" -ForegroundColor Green
Write-Host "You can now build the project with Gradle / Android Studio."
