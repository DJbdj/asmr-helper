#include "ui.h"
#include "platform.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cstdio>

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

#include "linenoise.h"

// ============== 补全过滤器 ==============

static FileFilter g_fileFilter = FILTER_ALL;

void setFileFilter(FileFilter filter) {
    g_fileFilter = filter;
}

static bool hasExtension(const std::string& name, const char* ext) {
    size_t extLen = strlen(ext);
    if (name.length() < extLen) return false;
    return name.compare(name.length() - extLen, extLen, ext) == 0;
}

static bool matchesFilter(const std::string& name, FileFilter filter) {
    if (filter == FILTER_ALL) return true;

    std::string lower = name;
    for (auto& c : lower) c = tolower(c);

    switch (filter) {
    case FILTER_VIDEO:
        return hasExtension(lower, ".mp4") || hasExtension(lower, ".mkv") ||
               hasExtension(lower, ".avi") || hasExtension(lower, ".mov") ||
               hasExtension(lower, ".webm") || hasExtension(lower, ".flv") ||
               hasExtension(lower, ".wmv");
    case FILTER_AUDIO:
        return hasExtension(lower, ".mp3") || hasExtension(lower, ".wav") ||
               hasExtension(lower, ".flac") || hasExtension(lower, ".aac") ||
               hasExtension(lower, ".m4a") || hasExtension(lower, ".ogg");
    case FILTER_IMAGE:
        return hasExtension(lower, ".jpg") || hasExtension(lower, ".jpeg") ||
               hasExtension(lower, ".png") || hasExtension(lower, ".bmp") ||
               hasExtension(lower, ".gif") || hasExtension(lower, ".webp");
    default:
        return true;
    }
}

// 生成默认输出文件名
std::string defaultOutputName(const std::string& inputPath) {
    std::string name = inputPath;

    // Find last path separator
    size_t lastSep = name.find_last_of("/\\");
    if (lastSep != std::string::npos) {
        name = name.substr(lastSep + 1);
    }

    // Replace extension with .mp4
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        name = name.substr(0, lastDot);
    }

    // Remove common temp suffixes like (1), _temp, etc.
    if (name.size() > 3 && name.compare(name.size() - 3, 3, "(1)") == 0) {
        name = name.substr(0, name.size() - 3);
    }

    // Remove trailing spaces/dots
    while (!name.empty() && (name.back() == ' ' || name.back() == '.')) {
        name.pop_back();
    }

    if (name.empty()) name = "output";
    name += ".mp4";
    return name;
}

// ============== 屏幕与 UI ==============

void clearScreen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void printHeader(const std::string& title) {
    std::string border(title.length() + 4, '=');
    std::cout << COLOR_CYAN << COLOR_BOLD;
    std::cout << "\n  " << border << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "  " << border << COLOR_RESET << "\n" << std::endl;
}

void printMenu(const std::vector<std::string>& items) {
    for (size_t i = 0; i < items.size(); i++) {
        std::cout << COLOR_YELLOW << "  [" << (i + 1) << "] " << COLOR_RESET
                  << items[i] << std::endl;
    }
    std::cout << std::endl;
}

// ============== Linenoise 补全 ==============

// Check if a path is a directory
static bool isDirectory(const std::string& path) {
#ifdef _WIN32
    DWORD attrs = GetFileAttributesA(path.c_str());
    return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

// 路径补全回调
static void pathCompletion(const char* buf, linenoiseCompletions* lc) {
    std::string line(buf);

    // 找到当前输入中最后一个路径分隔符之后的位置
    size_t lastSlash = line.find_last_of("/\\");
    std::string dirname;
    std::string basename;
    std::string pathPrefix;

    if (lastSlash == std::string::npos) {
        dirname = ".";
        basename = line;
        pathPrefix = "";
    } else {
        pathPrefix = line.substr(0, lastSlash + 1);
        dirname = line.substr(0, lastSlash);
        if (dirname.empty()) dirname = ".";
        basename = line.substr(lastSlash + 1);
    }

    std::string basenameLower = basename;
    for (char& c : basenameLower) c = tolower(c);
    size_t basenameLen = basename.length();

    auto files = platformListFiles(dirname);
    std::vector<std::string> matchList;

    for (const auto& name : files) {
        if (name == "." || name == "..") continue;

        std::string nameLower = name;
        for (char& c : nameLower) c = tolower(c);
        if (nameLower.substr(0, basenameLen) == basenameLower) {
            // Apply file type filter
            if (!matchesFilter(name, g_fileFilter)) continue;

            std::string completion = pathPrefix + name;

            // If it's a directory, append separator for further navigation
            std::string fullPath = platformPathJoin(dirname, name);
            if (isDirectory(fullPath)) {
#ifdef _WIN32
                completion += "\\";
#else
                completion += "/";
#endif
            }

            matchList.push_back(completion);
        }
    }

    if (matchList.empty()) return;

    // Compute common prefix
    std::string commonPrefix = matchList[0];
    for (size_t i = 1; i < matchList.size() && !commonPrefix.empty(); i++) {
        size_t j = 0;
        while (j < commonPrefix.length() && j < matchList[i].length() &&
               tolower(commonPrefix[j]) == tolower(matchList[i][j])) {
            j++;
        }
        commonPrefix = commonPrefix.substr(0, j);
    }

    for (const auto& m : matchList) {
        linenoiseAddCompletion(lc, m.c_str());
    }
}

// ============== Linenoise 初始化 ==============

void initReadline() {
    linenoiseSetCompletionCallback(pathCompletion);
    linenoiseHistorySetMaxLen(100);
    linenoiseSetMultiLine(1);
}

std::string getUserInput(const std::string& prompt) {
    // Strip trailing colon/spaces — we add our own consistent suffix
    std::string trimmed = prompt;
    while (!trimmed.empty() && (trimmed.back() == ':' || trimmed.back() == ' ')) {
        trimmed.pop_back();
    }

    std::string displayPrompt = "  " + trimmed + ": ";

    char* input = linenoise(displayPrompt.c_str());

    if (!input) {
        return "5";  // 默认退出
    }

    std::string result(input);

    if (!result.empty()) {
        linenoiseHistoryAdd(input);
    }

    linenoiseFree(input);
    return result;
}

std::string getPathInput(const std::string& prompt) {
    // linenoise already has completion callback set globally
    std::string result = getUserInput(prompt);

    if (!result.empty() && (result.front() == '"' || result.front() == '\'')) {
        result = result.substr(1, result.length() - 2);
    }

    return result;
}

bool fileExists(const std::string& path) {
#ifdef _WIN32
    // On Windows, std::ifstream doesn't handle UTF-8 paths correctly.
    // Convert UTF-8 path to UTF-16 and use _wstat.
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) return false;
    std::wstring wpath(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);
    wpath.pop_back(); // remove null terminator
    struct _stat64 st;
    return _wstat64(wpath.c_str(), &st) == 0;
#else
    std::ifstream f(path);
    return f.good();
#endif
}

// ============== TempFileManager ==============

TempFileManager::TempFileManager() {
    // 注册 atexit 清理
    atexit([]() {
        TempFileManager::instance().cleanup();
    });
}

TempFileManager& TempFileManager::instance() {
    static TempFileManager inst;
    return inst;
}

std::string TempFileManager::createTempFile(const std::string& suffix) {
    std::string path;
    int fd = platformCreateTempFile(path, "asmr-");
    if (fd >= 0) {
#ifdef _WIN32
        _close(fd);
#else
        ::close(fd);
#endif
    }

    // Append suffix if needed (platform creates unique name)
    temp_files_.push_back(path);
    return path;
}

void TempFileManager::cleanup() {
    for (const auto& path : temp_files_) {
        remove(path.c_str());
    }
    temp_files_.clear();
}

TempFileManager::~TempFileManager() {
    cleanup();
}
