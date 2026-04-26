#Requires -Version 5.0
# ASMR Helper - FFmpeg Development Libraries Setup Script (Windows)
# Downloads FFmpeg MSVC static development libraries for building asmr-helper
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

# Check if already exists
if (Test-Path (Join-Path $DevDir "lib\avformat.lib")) {
    Write-Host "[OK] FFmpeg dev libraries already exist" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $DevDir"
    Write-Host ""
    Write-Host "Now you can build with: cmake -B build_static -DFFMPEG_STATIC=ON && cmake --build build_static"
    exit 0
}

# Create directory
if (-not (Test-Path $DevDir)) {
    New-Item -ItemType Directory -Path $DevDir | Out-Null
}

# Download URL (System233/ffmpeg-msvc-prebuilt, FFmpeg 7.1 GPL static build)
$FFmpegUrl = "https://github.com/System233/ffmpeg-msvc-prebuilt/releases/download/n7.1-241205/ffmpeg-n7.1-241205-gpl-amd64-static.zip"
$TempFile = Join-Path $DevDir "ffmpeg-dev-download.zip"

Write-Host "[INFO] Downloading FFmpeg dev libraries (~170MB)..." -ForegroundColor Yellow
Write-Host "[INFO] Source: System233/ffmpeg-msvc-prebuilt (FFmpeg 7.1 GPL)" -ForegroundColor Yellow
Write-Host ""

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

# The zip extracts to a subdirectory like "ffmpeg-n7.1-..." — move contents up
$ExtractedDirs = Get-ChildItem -Path $DevDir -Directory -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -like "ffmpeg-*" }

if ($ExtractedDirs) {
    $SourceDir = $ExtractedDirs[0].FullName
    Get-ChildItem -Path $SourceDir | ForEach-Object {
        Move-Item $_.FullName -Destination $DevDir -Force
    }
    Remove-Item $SourceDir -Recurse -Force -ErrorAction SilentlyContinue
}

# Verify
$HasLibs = (Test-Path (Join-Path $DevDir "lib\avformat.lib")) -or
           (Test-Path (Join-Path $DevDir "lib\avformat.a"))
$HasInclude = Test-Path (Join-Path $DevDir "include\libavformat\avformat.h")

if ($HasLibs -and $HasInclude) {
    Write-Host ""
    Write-Host "================================" -ForegroundColor Green
    Write-Host "  FFmpeg dev libraries ready!" -ForegroundColor Green
    Write-Host "================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $DevDir"
    Write-Host ""
    Write-Host "Now you can build with:" -ForegroundColor Yellow
    Write-Host "  cmake -B build_static -DFFMPEG_STATIC=ON" -ForegroundColor White
    Write-Host "  cmake --build build_static --config Release" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host "[!] FFmpeg dev libraries not found after extraction" -ForegroundColor Red
    Write-Host "    Expected: $DevDir\lib\ and $DevDir\include\" -ForegroundColor Red
    Write-Host ""
    Write-Host "Downloaded archive may have a different structure." -ForegroundColor Red
    Write-Host "Please check: https://github.com/System233/ffmpeg-msvc-prebuilt" -ForegroundColor Red
    exit 1
}
