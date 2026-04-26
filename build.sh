#!/bin/bash

# ASMR Helper - 一键构建脚本

set -e

COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[0;33m'
COLOR_BLUE='\033[0;34m'
COLOR_RED='\033[0;31m'
COLOR_RESET='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo -e "${COLOR_BLUE}"
echo "================================"
echo "  ASMR Helper 构建脚本"
echo "================================"
echo -e "${COLOR_RESET}"

# 1. 检查 FFmpeg
echo -e "${COLOR_YELLOW}[1/2] 检查 FFmpeg...${COLOR_RESET}"
FFMPEG_BIN="${PROJECT_DIR}/native/ffmpeg/ffmpeg"
FFMPEG_FOUND=false

# 先检查系统 ffmpeg
if command -v ffmpeg &> /dev/null; then
    echo -e "${COLOR_GREEN}  ✓ 系统 FFmpeg 已安装${COLOR_RESET}"
    ffmpeg -version | head -1
    FFMPEG_FOUND=true
fi

# 检查内置 ffmpeg
if [ -f "${FFMPEG_BIN}" ]; then
    echo -e "${COLOR_GREEN}  ✓ 内置 FFmpeg 已存在${COLOR_RESET}"
    "${FFMPEG_BIN}" -version | head -1
    FFMPEG_FOUND=true
fi

# 如果都没有，下载内置版本
if [ "$FFMPEG_FOUND" = false ]; then
    echo "  正在下载内置 FFmpeg..."
    bash "${PROJECT_DIR}/scripts/download-ffmpeg.sh"
fi

# 2. 构建 C++ 程序
echo ""
echo -e "${COLOR_YELLOW}[2/2] 构建 C++ 程序...${COLOR_RESET}"

cd "${PROJECT_DIR}/native"

# 检查编译器
if ! command -v g++ &> /dev/null; then
    echo -e "${COLOR_RED}  错误：未找到 g++ 编译器${COLOR_RESET}"
    echo "  请安装：sudo apt-get install build-essential"
    exit 1
fi

echo -e "${COLOR_GREEN}  ✓ 编译器检查通过${COLOR_RESET}"

# 检查 libcurl
if ! pkg-config --exists libcurl 2>/dev/null; then
    echo -e "${COLOR_RED}  错误：未安装 libcurl 开发库${COLOR_RESET}"
    echo "  请安装：sudo apt-get install libcurl4-openssl-dev"
    exit 1
fi

# 检查 readline（用于终端输入）
if ! pkg-config --exists readline 2>/dev/null && [ ! -f /usr/include/readline/readline.h ]; then
    echo -e "${COLOR_RED}  错误：未安装 readline 开发库${COLOR_RESET}"
    echo "  请安装：sudo apt-get install libreadline-dev"
    exit 1
fi

echo -e "${COLOR_GREEN}  ✓ 依赖检查通过${COLOR_RESET}"

# 编译（先创建临时目录）
mkdir -p "${PROJECT_DIR}/native/.tmp"
make clean 2>/dev/null || true
make -j$(nproc 2>/dev/null || echo 2)

if [ -f "${PROJECT_DIR}/native/asmr-helper" ]; then
    echo -e "${COLOR_GREEN}  ✓ C++ 程序构建完成${COLOR_RESET}"
else
    echo -e "${COLOR_RED}  ✗ 构建失败${COLOR_RESET}"
    exit 1
fi

# 完成
echo ""
echo -e "${COLOR_GREEN}================================${COLOR_RESET}"
echo -e "${COLOR_GREEN}  构建完成!${COLOR_RESET}"
echo -e "${COLOR_GREEN}================================${COLOR_RESET}"
echo ""
echo "运行方式："
echo "  ./native/asmr-helper"
echo ""
echo "TypeScript CLI（如需构建）："
echo "  cd ts && npm install && npm run build"
echo ""