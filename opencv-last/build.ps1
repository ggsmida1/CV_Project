# ============================================================
# Character Defect Detection - One-Click Build Script
#
# Usage (PowerShell):
#   cd opencv-last
#   .\build.ps1              # incremental build (fast when code unchanged)
#   .\build.ps1 --clean      # clean build: delete build\ and bin\ first
#
# BEFORE FIRST RUN: Edit the 5 paths below to match your machine.
# Qt and OpenCV must be installed (MinGW version, not MSVC).
# OPENCV_DIR env variable is auto-set; you can also set it externally.
#
# All build artifacts go to build\; the executable and DLLs go to bin\.
# ============================================================

# ============================================================
# CONFIG - EDIT THESE PATHS TO MATCH YOUR MACHINE
# Team developers: change these 5 paths before the first run.
# Defaults below are examples; replace with your actual install locations.
# ============================================================
# Qt binary folder (has qmake.exe and Qt5*.dll)
$QtBinDir       = "E:\software-e\Qt5.14.2\5.14.2\mingw73_64\bin"
# Qt plugins folder (contains "platforms", "imageformats", ...)
$QtPluginsDir   = "E:\software-e\Qt5.14.2\5.14.2\mingw73_64\plugins"
# MinGW toolchain bin folder (has mingw32-make.exe, g++.exe)
$MingwBinDir    = "E:\software-e\Qt5.14.2\Tools\mingw730_64\bin"
# OpenCV runtime bin folder (has libopencv_world*.dll)
$OpenCvBinDir   = "E:\software-e\opencv-4.10.0\build_mingw\install\x64\mingw\bin"
# OpenCV root (same as OPENCV_DIR; must contain include\ and x64\mingw\lib\)
$OpenCvRoot     = "E:\software-e\opencv-4.10.0\build_mingw\install"
# ============================================================

# ---------- Parse arguments ----------
$cleanBuild = $false
foreach ($arg in $args) {
    if ($arg -eq "--clean" -or $arg -eq "-c") {
        $cleanBuild = $true
    }
}

$ProFile        = "opencv-last.pro"
$ExeName        = "CharacterDefectDetection.exe"
$ProjectDir     = $PSScriptRoot
$BuildDir       = "$ProjectDir\build"
$BinDir         = "$ProjectDir\bin"

# ---------- Validate paths ----------
function Test-DirExists([string]$path, [string]$name) {
    if (-not (Test-Path $path)) {
        Write-Host ""
        Write-Host "ERROR: [$name] path not found: $path" -ForegroundColor Red
        Write-Host "Please edit the CONFIG section at the top of build.ps1" -ForegroundColor Yellow
        exit 1
    }
}

Test-DirExists $QtBinDir     "Qt bin"
Test-DirExists $QtPluginsDir "Qt plugins"
Test-DirExists $MingwBinDir  "MinGW"
Test-DirExists $OpenCvBinDir "OpenCV bin"
Test-DirExists $OpenCvRoot   "OpenCV root"
Test-DirExists $ProjectDir   "Project"

# ---------- Set environment PATH ----------
$env:PATH = "$QtBinDir;$MingwBinDir;$OpenCvBinDir;$env:PATH"
$env:OPENCV_DIR = $OpenCvRoot

Write-Host ""
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host "  Character Defect Detection - Build Script"    -ForegroundColor Cyan
Write-Host "==============================================" -ForegroundColor Cyan
Write-Host ""

# ---------- Clean build if requested ----------
if ($cleanBuild) {
    Write-Host "[pre] --clean: removing build\ and bin\ ..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) { Remove-Item $BuildDir -Recurse -Force }
    if (Test-Path $BinDir)   { Remove-Item $BinDir   -Recurse -Force }
    Write-Host "  OK" -ForegroundColor Green
    Write-Host ""
}

# ---------- Step 1: qmake in build\ (only if Makefile missing or .pro changed) ----------
Write-Host "[1/4] Checking Makefile (qmake in build\)..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
$proTime = (Get-Item "$ProjectDir\$ProFile").LastWriteTimeUtc
$needQmake = $false
if (-not (Test-Path "$BuildDir\Makefile")) {
    $needQmake = $true
    Write-Host "  Makefile not found, running qmake..."
} else {
    $makeTime = (Get-Item "$BuildDir\Makefile").LastWriteTimeUtc
    if ($proTime -gt $makeTime) {
        $needQmake = $true
        Write-Host "  $ProFile changed, re-running qmake..."
    } else {
        Write-Host "  Makefile up-to-date, skip qmake"
    }
}
if ($needQmake) {
    Set-Location $BuildDir
    if (Test-Path "Makefile") {
        Remove-Item "Makefile" -Force
    }
    $proc = Start-Process -FilePath "qmake" -ArgumentList "..\$ProFile" -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) {
        Write-Host "qmake failed (exit=$($proc.ExitCode))" -ForegroundColor Red
        exit 1
    }
    if (-not (Test-Path "Makefile")) {
        Write-Host "qmake reported success but Makefile not found in build\" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  OK" -ForegroundColor Green

# ---------- Step 2: Incremental compile (no clean - let make decide) ----------
Write-Host "[2/4] Compiling (mingw32-make in build\)..." -ForegroundColor Yellow
Set-Location $BuildDir
$proc = Start-Process -FilePath "mingw32-make" -Wait -PassThru -NoNewWindow
if ($proc.ExitCode -ne 0) {
    Write-Host "Build failed (exit=$($proc.ExitCode))" -ForegroundColor Red
    exit 1
}
Write-Host "  OK" -ForegroundColor Green

# ---------- Step 3: Copy DLLs to bin\ ----------
Write-Host "[3/4] Copying dependency DLLs to bin\ ..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $BinDir -Force | Out-Null

$qtDlls = @("Qt5Core.dll", "Qt5Gui.dll", "Qt5Widgets.dll")
$copied = 0
foreach ($dll in $qtDlls) {
    $src = "$QtBinDir\$dll"
    $dst = "$BinDir\$dll"
    if ((Test-Path $src) -and -not (Test-Path $dst)) {
        Copy-Item $src $dst -Force
        $copied++
    }
}

$opencvDlls = @("libopencv_world4100.dll", "opencv_videoio_ffmpeg4100_64.dll")
foreach ($dll in $opencvDlls) {
    $src = "$OpenCvBinDir\$dll"
    $dst = "$BinDir\$dll"
    if ((Test-Path $src) -and -not (Test-Path $dst)) {
        Copy-Item $src $dst -Force
        $copied++
    }
}

$mingwDlls = @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")
foreach ($dll in $mingwDlls) {
    $src = "$MingwBinDir\$dll"
    $dst = "$BinDir\$dll"
    if ((Test-Path $src) -and -not (Test-Path $dst)) {
        Copy-Item $src $dst -Force
        $copied++
    }
}

# Qt platform plugin (qwindows.dll) - required on Windows
$platformsSrc = "$QtPluginsDir\platforms"
$platformsDst = "$BinDir\platforms"
if ((Test-Path $platformsSrc) -and -not (Test-Path $platformsDst)) {
    Copy-Item $platformsSrc $platformsDst -Recurse
    $copied++
}

Write-Host "  Copied $copied new item(s)" -ForegroundColor Green

# ---------- Done ----------
$exePath = "$BinDir\$ExeName"
if (-not (Test-Path $exePath)) {
    Write-Host ""
    Write-Host "Build OK but $exeName not found in bin\" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "==============================================" -ForegroundColor Green
Write-Host "  BUILD SUCCESS!"                               -ForegroundColor Green
Write-Host "  Executable: $exePath"                          -ForegroundColor Green
Write-Host "==============================================" -ForegroundColor Green
Write-Host ""
Write-Host "Launching program..." -ForegroundColor Cyan
Start-Process $exePath -WorkingDirectory $BinDir
