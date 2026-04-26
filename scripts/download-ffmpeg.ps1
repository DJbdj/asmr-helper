# ASMR Helper - FFmpeg Download Script (Windows PowerShell)
# Downloads FFmpeg binaries for Windows x64

$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "================================" -ForegroundColor Blue
Write-Host "  FFmpeg 下载脚本 (Windows)" -ForegroundColor Blue
Write-Host "================================" -ForegroundColor Blue
Write-Host ""

# 确定项目根目录（脚本在 scripts/ 目录下）
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectDir = Split-Path -Parent $ScriptDir
$FFmpegDir = Join-Path $ProjectDir "native\ffmpeg"
$FFmpegBin = Join-Path $FFmpegDir "ffmpeg.exe"

# 检查 FFmpeg 是否已存在
if (Test-Path $FFmpegBin) {
    Write-Host "FFmpeg 已存在" -ForegroundColor Green
    Write-Host ""
    Write-Host "位置: $FFmpegBin"
    & $FFmpegBin -version | Select-Object -First 1
    exit 0
}

# 创建目录
if (-not (Test-Path $FFmpegDir)) {
    New-Item -ItemType Directory -Path $FFmpegDir -Force | Out-Null
}

$FFmpegUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-git-essentials.7z"
$TempDir = Join-Path $env:TEMP "asmr-ffmpeg-download"

Write-Host "系统: Windows x64" -ForegroundColor Yellow
Write-Host ""
Write-Host "下载地址: $FFmpegUrl"
Write-Host ""

# 下载
$TempFile = Join-Path $env:TEMP "asmr-ffmpeg.7z"

try {
    Write-Host "正在下载..."
    Invoke-WebRequest -Uri $FFmpegUrl -OutFile $TempFile -UseBasicParsing
} catch {
    Write-Host "下载失败: $_" -ForegroundColor Red
    exit 1
}

# 解压
try {
    Write-Host "正在解压..."

    # 确保临时目录存在
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
    New-Item -ItemType Directory -Path $TempDir -Force | Out-Null

    # 使用 7z 或 PowerShell 解压
    if (Get-Command 7z -ErrorAction SilentlyContinue) {
        7z x $TempFile -o"$TempDir" -y | Out-Null
    } else {
        # PowerShell 不支持 .7z 原生，尝试用 Expand-Archive 或 curl
        # 改为下载 zip 版本
        Write-Host "未找到 7z，改用 zip 版本..." -ForegroundColor Yellow
        Remove-Item $TempFile -Force

        $ZipUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-git-essentials.zip"
        $TempZipFile = Join-Path $env:TEMP "asmr-ffmpeg.zip"
        Invoke-WebRequest -Uri $ZipUrl -OutFile $TempZipFile -UseBasicParsing

        Write-Host "正在解压 zip..."
        Expand-Archive -Path $TempZipFile -DestinationPath $TempDir -Force
        Remove-Item $TempZipFile -Force
    }

    # 查找 ffmpeg.exe 和 ffprobe.exe
    $FFmpegExe = Get-ChildItem -Path $TempDir -Filter "ffmpeg.exe" -Recurse | Select-Object -First 1
    $FFprobeExe = Get-ChildItem -Path $TempDir -Filter "ffprobe.exe" -Recurse | Select-Object -First 1

    if ($FFmpegExe) {
        Copy-Item $FFmpegExe.FullName -Destination (Join-Path $FFmpegDir "ffmpeg.exe") -Force
        Write-Host "已提取 ffmpeg.exe"
    }

    if ($FFprobeExe) {
        Copy-Item $FFprobeExe.FullName -Destination (Join-Path $FFmpegDir "ffprobe.exe") -Force
        Write-Host "已提取 ffprobe.exe"
    }

    # 清理
    if (Test-Path $TempDir) {
        Remove-Item -Recurse -Force $TempDir
    }
    if (Test-Path $TempFile) {
        Remove-Item $TempFile -Force
    }

    Write-Host ""
    Write-Host "================================" -ForegroundColor Green
    Write-Host "  FFmpeg 下载完成！" -ForegroundColor Green
    Write-Host "================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "位置: $FFmpegDir"
    if (Test-Path $FFmpegBin) {
        & $FFmpegBin -version | Select-Object -First 1
    }
    Write-Host ""
    Write-Host "现在可以运行 asmr-helper.exe"

} catch {
    Write-Host "解压失败: $_" -ForegroundColor Red
    exit 1
}
