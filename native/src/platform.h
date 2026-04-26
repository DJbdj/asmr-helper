#pragma once

#include <string>
#include <vector>

// 获取可执行文件所在目录
std::string platformGetExecutableDir();

// 切换工作目录
int platformChdir(const std::string& path);

// 获取临时目录（Unix: /tmp, Windows: %TEMP%）
std::string platformGetTempDir();

// 创建临时文件，返回文件路径和文件描述符（Unix: fd, Windows: handle 强转为 int）
int platformCreateTempFile(std::string& outPath, const std::string& prefix);

// 列出目录中的所有文件（不含 . 和 ..）
std::vector<std::string> platformListFiles(const std::string& dir);

// 路径拼接（自动选择 / 或 \）
std::string platformPathJoin(const std::string& a, const std::string& b);

// 查找可执行文件路径（Unix: which, Windows: where）
std::string platformFindCommand(const std::string& cmd);

// 获取二进制文件名（Windows 自动追加 .exe）
std::string platformGetBinaryName(const std::string& name);

// 初始化控制台（Windows: 启用 VT 处理，Unix: 空操作）
void platformInitConsole();

// 控制台颜色设置（跨平台）
void platformSetConsoleColor(int colorCode);
void platformResetConsoleColor();

// 运行 FFmpeg 下载脚本
int platformRunDownloadScript();

// 获取平台空设备路径（Unix: /dev/null, Windows: nul）
const char* platformDevNull();
