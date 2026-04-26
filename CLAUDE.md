# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ASMR Helper — a tool that replaces video frames with an image (keeping audio) or generates video from audio + image. Two frontends: an interactive C++ CLI and a TypeScript CLI. Both use FFmpeg runtime binaries and `ffprobe` for media info.

## Directory Structure

```
asmr-helper/
├── build.sh                 # One-command build script (FFmpeg download + C++ compile)
├── native/
│   ├── Makefile             # C++ build: src/*.cpp -> build/*.o -> asmr-helper
│   ├── src/
│   │   ├── main.cpp         # Entry point + menu routing
│   │   ├── ui.h / ui.cpp    # Colors, screen I/O, readline, TempFileManager (/tmp/asmr-*)
│   │   ├── http.h / http.cpp # CURL, Pexels API, JSON parsing, image download
│   │   └── video.h / video.cpp # ffprobe media info, ffmpeg processing, progress bars, menus
│   └── ffmpeg/              # Bundled FFmpeg binary (downloaded by scripts/)
├── ts/
│   ├── src/
│   │   ├── index.ts         # CLI entry (commander): video/audio/search/info commands
│   │   ├── backend.ts       # Wraps ffmpeg/ffprobe/curl via child_process
│   │   └── types.ts         # Shared TypeScript types
│   └── package.json
└── scripts/
    └── download-ffmpeg.sh   # Downloads platform-appropriate FFmpeg binary
```

## Build & Run

### C++ (interactive CLI)
```bash
./build.sh                      # Full build: FFmpeg + C++ compile
cd native && make clean && make # C++ only
./native/asmr-helper            # Run interactive menu
```

Dependencies: `g++`, `libcurl4-openssl-dev`, `libreadline-dev`. No FFmpeg dev libraries needed at compile time.

### TypeScript CLI
```bash
cd ts && npm install && npm run build
npm start -- video -i input.mp4 -q nature -o output.mp4
npm start -- audio -i audio.mp3 -q wallpaper -o video.mp4
npm start -- search -q nature
npm start -- info -i input.mp4
```

## Key Architecture Decisions

- **FFmpeg strategy**: C++ code uses `ffprobe` JSON output (via `popen`) for media info and `ffmpeg` binary for processing — same approach as the TypeScript backend. No FFmpeg C library dependencies at compile time.
- **Temp file management**: `TempFileManager` singleton (`ui.cpp`) creates files in `/tmp/asmr-XXXXXX` via `mkstemp`, registered for cleanup via `atexit()`.
- **Progress bars**: `video.cpp` uses `-nostats -progress pipe:2` to parse `time=` from ffmpeg output, rendering ` [████████░░░░░░░░] 45.2% (00:32 / 01:12)`.
- **Pexels API key** is hardcoded in both `http.cpp` and `backend.ts`.
- The Makefile uses `$(wildcard $(SRCDIR)/*.cpp)` — adding new `.cpp` files requires no Makefile changes.
