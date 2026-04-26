# ASMR Helper

视频源替换工具 - 将视频画面替换为图片，或从音频生成视频。

## 功能

- **视频替换画面** - 将视频画面替换为指定图片，保留原音频
- **音频生成视频** - 用图片作为背景，生成带有音频的视频
- **Pexels 图片搜索** - 支持高质量壁纸搜索
- **FFmpeg 内置** - 无需额外安装 FFmpeg

## 快速开始

### 1. 构建程序

```bash
chmod +x build.sh
./build.sh
```

构建脚本会自动：
- 下载 FFmpeg 到程序目录
- 编译 C++ 程序
- 编译 TypeScript CLI（可选）

### 2. 运行程序

```bash
./native/asmr-helper
```

## 使用方式

### 交互式界面

运行程序后，按照引导菜单操作：

```
================================
  ASMR Helper - 视频源替换工具
================================

  [1] 视频替换画面（保留音频）
  [2] 音频生成视频（新功能）
  [3] 下载 FFmpeg
  [4] 查看帮助
  [5] 退出程序
```

### TypeScript CLI（可选）

如果安装了 Node.js：

```bash
cd ts
npm install
npm start -- video -i input.mp4 -q nature -o output.mp4
npm start -- audio -i audio.mp3 -q wallpaper -o video.mp4
```

## 图片来源

1. **Pexels 搜索** - 输入英文关键词搜索高质量壁纸
2. **图片 URL** - 直接使用网络图片 URL
3. **本地文件** - 使用本地图片文件

## 支持格式

- **视频**: mp4, mkv, avi, mov, webm
- **音频**: mp3, wav, flac, aac, m4a
- **图片**: jpg, png, bmp, gif

## 测试样例

design.md 中提供的测试数据：

- 视频: `/mnt/d/Users/liyif/Downloads/IDM/椰椰.mp4`
- 图片 URL: `https://img-s.msn.cn/tenant/amp/entityid/AA21qn2c.img?w=768&h=1039&m=6`

## 系统要求

- Linux (x86_64 或 aarch64)
- macOS (可选)
- C++17 编译器
- FFmpeg 开发库（可选，可使用内置版本）
- libcurl（可选）

### 安装依赖（可选）

如果需要使用 FFmpeg 开发库而不是内置版本：

```bash
sudo apt-get install build-essential libavformat-dev libavcodec-dev libswscale-dev libcurl4-openssl-dev
```

## 项目结构

```
asmr-helper/
├── build.sh            # 构建脚本
├── design.md           # 设计文档
├── native/
│   ├── Makefile        # 构建配置
│   ├── src/main.cpp    # C++ 核心代码
│   └── ffmpeg/         # 内置 FFmpeg
├── ts/                 # TypeScript CLI
│   ├── package.json
│   ├── tsconfig.json
│   └── src/
└── scripts/
    └── download-ffmpeg.sh
```

## API 配置

使用 Pexels API 获取图片：
- API 文档: https://www.pexels.com/api/documentation/
- API Key 已内置在程序中