#!/bin/bash

# ASMR Helper - FFmpeg 下载脚本
# 将 FFmpeg 下载到程序目录，实现完全内置

set -e

COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[0;33m'
COLOR_BLUE='\033[0;34m'
COLOR_RED='\033[0;31m'
COLOR_RESET='\033[0m'

echo -e "${COLOR_BLUE}"
echo "================================"
echo "  FFmpeg 下载脚本"
echo "================================"
echo -e "${COLOR_RESET}"

# 确定项目根目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "${SCRIPT_DIR}")"

# FFmpeg 目标位置（native 目录下，与程序同级）
FFMPEG_DIR="${PROJECT_DIR}/native/ffmpeg"
FFMPEG_BIN="${FFMPEG_DIR}/ffmpeg"

# 检查 FFmpeg 是否已存在
if [ -f "${FFMPEG_BIN}" ]; then
    echo -e "${COLOR_GREEN}✓ FFmpeg 已存在${COLOR_RESET}"
    echo ""
    echo "位置: ${FFMPEG_BIN}"
    echo "版本: $(${FFMPEG_BIN} -version | head -1)"
    exit 0
fi

# 创建目录
mkdir -p "${FFMPEG_DIR}"

# 检测系统
ARCH=$(uname -m)
OS=$(uname -s)

echo -e "${COLOR_YELLOW}系统: ${OS} ${ARCH}${COLOR_RESET}"

# 选择下载 URL
if [ "${OS}" = "Linux" ]; then
    if [ "${ARCH}" = "x86_64" ]; then
        FFMPEG_URL="https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz"
    elif [ "${ARCH}" = "aarch64" ]; then
        FFMPEG_URL="https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-arm64-static.tar.xz"
    else
        echo -e "${COLOR_RED}不支持的架构: ${ARCH}${COLOR_RESET}"
        exit 1
    fi
elif [ "${OS}" = "Darwin" ]; then
    FFMPEG_URL="https://evermeet.cx/ffmpeg/getrelease/zip"
else
    echo -e "${COLOR_RED}不支持的系统: ${OS}${COLOR_RESET}"
    exit 1
fi

echo ""
echo "下载地址: ${FFMPEG_URL}"
echo ""

# 下载
cd "${FFMPEG_DIR}"
TEMP_FILE="ffmpeg-download.tmp"

echo "正在下载..."
if command -v wget &> /dev/null; then
    wget -q --show-progress "${FFMPEG_URL}" -O "${TEMP_FILE}"
else
    curl -# -L "${FFMPEG_URL}" -o "${TEMP_FILE}"
fi

echo ""
echo "正在解压..."

# 根据文件类型解压
if [[ "${FFMPEG_URL}" == *.tar.xz ]]; then
    tar -xf "${TEMP_FILE}"
    # 查找解压后的目录
    EXTRACTED_DIR=$(ls -d ffmpeg-*-amd64-static ffmpeg-*-arm64-static 2>/dev/null | head -1)
    if [ -n "${EXTRACTED_DIR}" ]; then
        mv "${EXTRACTED_DIR}/ffmpeg" "${FFMPEG_BIN}"
        mv "${EXTRACTED_DIR}/ffprobe" "${FFMPEG_DIR}/ffprobe" 2>/dev/null || true
        rm -rf "${EXTRACTED_DIR}"
    fi
elif [[ "${FFMPEG_URL}" == *.zip ]]; then
    unzip -q "${TEMP_FILE}"
    # macOS 版本直接是 ffmpeg 文件
    if [ -f "ffmpeg" ]; then
        mv ffmpeg "${FFMPEG_BIN}"
    fi
fi

rm -f "${TEMP_FILE}"
chmod +x "${FFMPEG_BIN}" 2>/dev/null || true

echo ""
echo -e "${COLOR_GREEN}================================${COLOR_RESET}"
echo -e "${COLOR_GREEN}  FFmpeg 下载完成!${COLOR_RESET}"
echo -e "${COLOR_GREEN}================================${COLOR_RESET}"
echo ""
echo "位置: ${FFMPEG_BIN}"
echo "版本: $(${FFMPEG_BIN} -version 2>/dev/null | head -1 || echo '未知')"
echo ""
echo "现在可以运行 ./build.sh 构建程序"