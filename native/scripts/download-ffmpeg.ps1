#Requires -Version 5.0
# ASMR Helper - FFmpeg Download Script (Windows)
# Downloads Windows FFmpeg static build from gyan.dev

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================" -ForegroundColor Cyan
Write-Host "  FFmpeg Download Script" -ForegroundColor Cyan
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

# Determine script location and project root
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectDir = Split-Path -Parent $ScriptDir

# FFmpeg target directory (native/ffmpeg/)
$FFmpegDir = Join-Path $ProjectDir "ffmpeg"
$FFmpegBin = Join-Path $FFmpegDir "ffmpeg.exe"
$FFProbeBin = Join-Path $FFmpegDir "ffprobe.exe"

# Check if FFmpeg already exists
if (Test-Path $FFmpegBin) {
    Write-Host "[OK] FFmpeg already exists" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $FFmpegBin"
    $version = & $FFmpegBin -version 2>&1 | Select-Object -First 1
    Write-Host "Version: $version"
    exit 0
}

# Create directory
if (-not (Test-Path $FFmpegDir)) {
    New-Item -ItemType Directory -Path $FFmpegDir | Out-Null
}

# Detect architecture
$Arch = $env:PROCESSOR_ARCHITECTURE
if ($Arch -eq "AMD64") {
    $FFmpegUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.7z"
} elseif ($Arch -eq "ARM64") {
    Write-Host "[!] ARM64 detected — using x64 build (may not work natively)" -ForegroundColor Yellow
    $FFmpegUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.7z"
} else {
    Write-Host "[!] Unsupported architecture: $Arch" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] Architecture: $Arch" -ForegroundColor Yellow
Write-Host "[INFO] Download URL: $FFmpegUrl" -ForegroundColor Yellow
Write-Host ""

# Download
$TempFile = Join-Path $FFmpegDir "ffmpeg-download.7z"

Write-Host "  Downloading..." -ForegroundColor Yellow
try {
    Invoke-WebRequest -Uri $FFmpegUrl -OutFile $TempFile -UseBasicParsing
} catch {
    Write-Host "[!] Download failed: $_" -ForegroundColor Red
    exit 1
}

Write-Host "  Extracting..." -ForegroundColor Yellow

# Extract 7z archive
try {
    # Try 7-Zip first (if installed)
    $SevenZip = $null
    $SevenZipPaths = @(
        "C:\Program Files\7-Zip\7z.exe",
        "C:\Program Files (x86)\7-Zip\7z.exe",
        "$env:ProgramFiles\7-Zip\7z.exe"
    )
    foreach ($path in $SevenZipPaths) {
        if (Test-Path $path) {
            $SevenZip = $path
            break
        }
    }

    if ($SevenZip) {
        & $SevenZip x $TempFile "-o$FFmpegDir" -y | Out-Null
    } elseif (Get-Command "7z" -ErrorAction SilentlyContinue) {
        7z x $TempFile "-o$FFmpegDir" -y | Out-Null
    } else {
        # Try using Expand-Archive (won't work for 7z, but try tar as fallback)
        # Windows 10+ supports tar command
        tar -xf $TempFile -C $FFmpegDir 2>$null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "[!] No 7-Zip found and tar extraction failed." -ForegroundColor Red
            Write-Host "    Please install 7-Zip from https://www.7-zip.org/" -ForegroundColor Red
            Write-Host "    Or manually download and extract FFmpeg to: $FFmpegDir" -ForegroundColor Red
            Remove-Item $TempFile -Force -ErrorAction SilentlyContinue
            exit 1
        }
    }
} catch {
    Write-Host "[!] Extraction failed: $_" -ForegroundColor Red
    Remove-Item $TempFile -Force -ErrorAction SilentlyContinue
    exit 1
}

# Clean up temp file
Remove-Item $TempFile -Force -ErrorAction SilentlyContinue

# Find ffmpeg.exe in extracted directory (structure: ffmpeg-XXXX-essentials_build/bin/ffmpeg.exe)
$ExtractedBins = Get-ChildItem -Path $FFmpegDir -Recurse -Filter "ffmpeg.exe" -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -notlike "*\ffmpeg-download*" }

if ($ExtractedBins) {
    $SourceBin = $ExtractedBins[0].FullName
    $SourceDir = Split-Path $SourceBin

    # Copy ffmpeg.exe and ffprobe.exe to native/ffmpeg/
    Copy-Item $SourceBin $FFmpegBin -Force
    $ProbeSource = Join-Path $SourceDir "ffprobe.exe"
    if (Test-Path $ProbeSource) {
        Copy-Item $ProbeSource $FFProbeBin -Force
    }

    # Clean up extracted directory
    $ExtractedDirs = Get-ChildItem -Path $FFmpegDir -Directory -ErrorAction SilentlyContinue
    foreach ($dir in $ExtractedDirs) {
        Remove-Item $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
    }
} else {
    # Check if files were extracted directly
    if (Test-Path (Join-Path $FFmpegDir "bin\ffmpeg.exe")) {
        Copy-Item (Join-Path $FFmpegDir "bin\ffmpeg.exe") $FFmpegBin -Force
        if (Test-Path (Join-Path $FFmpegDir "bin\ffprobe.exe")) {
            Copy-Item (Join-Path $FFmpegDir "bin\ffprobe.exe") $FFProbeBin -Force
        }
    }
}

# Verify
if (Test-Path $FFmpegBin) {
    Write-Host ""
    Write-Host "================================" -ForegroundColor Green
    Write-Host "  FFmpeg downloaded successfully!" -ForegroundColor Green
    Write-Host "================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Location: $FFmpegBin"
    $version = & $FFmpegBin -version 2>&1 | Select-Object -First 1
    Write-Host "Version: $version"
    Write-Host ""
    Write-Host "Now you can build and run the program."
} else {
    Write-Host "[!] FFmpeg binary not found after extraction" -ForegroundColor Red
    Write-Host "    Expected location: $FFmpegBin" -ForegroundColor Red
    exit 1
}
