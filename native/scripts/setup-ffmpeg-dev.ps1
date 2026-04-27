#Requires -Version 5.0
# ASMR Helper - FFmpeg Development Libraries Setup Script (Windows)
# Downloads FFmpeg MSVC shared development libraries (headers + import libs + DLLs)
# Source: System233/ffmpeg-msvc-prebuilt (https://github.com/System233/ffmpeg-msvc-prebuilt)

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================" -ForegroundColor Cyan
Write-Host "  FFmpeg Dev Libraries Setup" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Determine script location and project root
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectDir = Split-Path -Parent $ScriptDir

# Target directory for FFmpeg dev libraries
$DevDir = Join-Path $ProjectDir "ffmpeg-dev"

# Check if already exists (this package puts .lib in bin/, not lib/)
if ((Test-Path (Join-Path $DevDir "include\libavformat\avformat.h")) -and
    (Test-Path (Join-Path $DevDir "bin\avformat.lib"))) {
    Write-Host "[OK] FFmpeg dev libraries already exist" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $DevDir"
    Write-Host ""
    Write-Host "Now you can build with:" -ForegroundColor Yellow
    Write-Host "  Remove-Item -Recurse -Force build_static" -ForegroundColor White
    Write-Host "  cmake -B build_static -DFFMPEG_STATIC=ON" -ForegroundColor White
    Write-Host "  cmake --build build_static --config Release" -ForegroundColor White
    Write-Host ""
    exit 0
}

# Clean any partial installation
if (Test-Path $DevDir) {
    Remove-Item $DevDir -Recurse -Force -ErrorAction SilentlyContinue
}
New-Item -ItemType Directory -Path $DevDir | Out-Null

# Download URL: SHARED build (contains include/, lib/, bin/)
# Static builds only have executables — shared builds have dev libraries + DLLs
$FFmpegUrl = "https://github.com/System233/ffmpeg-msvc-prebuilt/releases/download/n7.1-241205/ffmpeg-n7.1-241205-gpl-amd64-shared.zip"

Write-Host "[INFO] Downloading FFmpeg dev libraries (~130MB)..." -ForegroundColor Yellow
Write-Host "[INFO] Source: System233/ffmpeg-msvc-prebuilt (FFmpeg 7.1 GPL shared)" -ForegroundColor Yellow
Write-Host "[INFO] This includes headers, import libraries, and runtime DLLs" -ForegroundColor Yellow
Write-Host ""

$TempFile = Join-Path $DevDir "ffmpeg-dev-download.zip"

try {
    Invoke-WebRequest -Uri $FFmpegUrl -OutFile $TempFile -UseBasicParsing
} catch {
    Write-Host "[!] Download failed: $_" -ForegroundColor Red
    Write-Host "    Please check your network connection and try again." -ForegroundColor Red
    Remove-Item $DevDir -Recurse -Force -ErrorAction SilentlyContinue
    exit 1
}

Write-Host "  Extracting..." -ForegroundColor Yellow

try {
    Expand-Archive -Path $TempFile -DestinationPath $DevDir -Force
} catch {
    Write-Host "[!] Extraction failed: $_" -ForegroundColor Red
    Remove-Item $TempFile -Force -ErrorAction SilentlyContinue
    Remove-Item $DevDir -Recurse -Force -ErrorAction SilentlyContinue
    exit 1
}

# Clean up temp file
Remove-Item $TempFile -Force -ErrorAction SilentlyContinue

# The zip extracts to a subdirectory like "ffmpeg-n7.1-...-shared" — move contents up
$ExtractedDirs = Get-ChildItem -Path $DevDir -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "ffmpeg-*" }

if ($ExtractedDirs) {
    $SourceDir = $ExtractedDirs[0].FullName
    Get-ChildItem -Path $SourceDir | ForEach-Object {
        Move-Item $_.FullName -Destination $DevDir -Force
    }
    Remove-Item $SourceDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Verify (.lib files are in bin/ alongside DLLs in this package)
$HasInclude = Test-Path (Join-Path $DevDir "include\libavformat\avformat.h")
$HasLibs = Test-Path (Join-Path $DevDir "bin\avformat.lib")
$HasDLLs = Test-Path (Join-Path $DevDir "bin\avformat-61.dll")

if ($HasInclude -and $HasLibs) {
    Write-Host ""
    Write-Host "================================" -ForegroundColor Green
    Write-Host "  FFmpeg dev libraries ready!" -ForegroundColor Green
    Write-Host "================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $DevDir"
    Write-Host ""
    Write-Host "  include/  — header files for compilation" -ForegroundColor White
    Write-Host "  bin/      — DLLs + import libraries for linking" -ForegroundColor White
    Write-Host ""
    Write-Host "Build commands:" -ForegroundColor Yellow
    Write-Host "  Remove-Item -Recurse -Force build_static  # Important: clear old cache" -ForegroundColor White
    Write-Host "  cmake -B build_static -DFFMPEG_STATIC=ON" -ForegroundColor White
    Write-Host "  cmake --build build_static --config Release" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "[!] FFmpeg dev libraries not found after extraction" -ForegroundColor Red
    Write-Host "    Expected: $DevDir\include\ and $DevDir\lib\" -ForegroundColor Red
    Write-Host ""
    Write-Host "Downloaded archive may have a different structure." -ForegroundColor Red
    Write-Host "Please check: https://github.com/System233/ffmpeg-msvc-prebuilt/releases" -ForegroundColor Red
    exit 1
}
