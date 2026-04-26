#include "platform.h"
#include <cstdio>
#include <cstring>

// Internal color codes (matching ANSI values, avoids conflict with ui.h macros)
enum PlatformColor {
    PC_RESET  = 0,
    PC_GREEN  = 32,
    PC_YELLOW = 33,
    PC_BLUE   = 34,
    PC_CYAN   = 36,
    PC_RED    = 31,
    PC_BOLD   = 1,
};

#ifdef _WIN32
    #include <windows.h>
    #include <shlwapi.h>
    #include <direct.h>
    #include <io.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #define popen  _popen
    #define pclose _pclose
    #define access _access
    #define open   _open
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <libgen.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <linux/limits.h>
#endif

// ============== getExecutableDir ==============

std::string platformGetExecutableDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string path(buf);
    size_t pos = path.find_last_of('\\');
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
#else
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len == -1) return "";
    buf[len] = '\0';
    std::string path(buf);
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
#endif
}

// ============== Chdir ==============

int platformChdir(const std::string& path) {
#ifdef _WIN32
    return _chdir(path.c_str());
#else
    return chdir(path.c_str());
#endif
}

// ============== GetTempDir ==============

std::string platformGetTempDir() {
#ifdef _WIN32
    char buf[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, buf);
    if (len > 0 && len < MAX_PATH) {
        std::string result(buf);
        // Remove trailing backslash
        if (!result.empty() && result.back() == '\\') {
            result.pop_back();
        }
        return result;
    }
    return "C:\\Temp";
#else
    return "/tmp";
#endif
}

// ============== CreateTempFile ==============

int platformCreateTempFile(std::string& outPath, const std::string& prefix) {
    std::string tempDir = platformGetTempDir();

#ifdef _WIN32
    char tempPath[MAX_PATH];
    // GetTempFileName requires the prefix to be at most 3 chars
    char pfx[4] = {0};
    std::strncpy(pfx, prefix.c_str(), 3);

    UINT ret = GetTempFileNameA(tempDir.c_str(), pfx, 0, tempPath);
    if (ret == 0) return -1;

    outPath = tempPath;

    int fd = open(tempPath, _O_WRONLY | _O_CREAT | _O_TRUNC, _S_IREAD | _S_IWRITE);
    return fd;
#else
    std::string tmpl = tempDir + "/" + prefix + "XXXXXX";
    std::vector<char> buf(tmpl.begin(), tmpl.end());
    buf.push_back('\0');

    int fd = mkstemp(buf.data());
    if (fd < 0) return -1;

    outPath = buf.data();
    return fd;
#endif
}

// ============== ListFiles ==============

std::vector<std::string> platformListFiles(const std::string& dir) {
    std::vector<std::string> result;

#ifdef _WIN32
    std::string pattern = dir + "\\*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return result;

    do {
        std::string name(findData.cFileName);
        if (name == "." || name == "..") continue;
        // Skip hidden files (starting with .)
        if (!name.empty() && name[0] == '.') continue;
        result.push_back(name);
    } while (FindNextFileA(hFind, &findData) != 0);

    FindClose(hFind);
#else
    DIR* d = opendir(dir.c_str());
    if (!d) return result;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (!name.empty() && name[0] == '.') continue;
        result.push_back(name);
    }
    closedir(d);
#endif

    return result;
}

// ============== PathJoin ==============

std::string platformPathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;

#ifdef _WIN32
    bool aEndsSep = (a.back() == '\\' || a.back() == '/');
    bool bStartsSep = (b.front() == '\\' || b.front() == '/');

    if (aEndsSep && bStartsSep) return a + b.substr(1);
    if (!aEndsSep && !bStartsSep) return a + "\\" + b;
    return a + b;
#else
    bool aEndsSep = (a.back() == '/');
    bool bStartsSep = (b.front() == '/');

    if (aEndsSep && bStartsSep) return a + b.substr(1);
    if (!aEndsSep && !bStartsSep) return a + "/" + b;
    return a + b;
#endif
}

// ============== FindCommand ==============

std::string platformFindCommand(const std::string& cmd) {
#ifdef _WIN32
    std::string fullCmd = "where " + cmd + " 2>nul";
#else
    std::string fullCmd = "which " + cmd + " 2>/dev/null";
#endif

    FILE* pipe = popen(fullCmd.c_str(), "r");
    if (!pipe) return "";

    char buffer[4096];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = buffer;
    }
    pclose(pipe);

    // Trim trailing whitespace/newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'
            || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }

    return result;
}

// ============== GetBinaryName ==============

std::string platformGetBinaryName(const std::string& name) {
#ifdef _WIN32
    // If already has .exe suffix, return as-is
    if (name.size() >= 4 && name.substr(name.size() - 4) == ".exe") {
        return name;
    }
    return name + ".exe";
#else
    return name;
#endif
}

// ============== Console Init ==============

void platformInitConsole() {
#ifdef _WIN32
    // Set console code page to UTF-8 for proper Chinese character display
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;
    DWORD mode = 0;
    if (GetConsoleMode(hConsole, &mode)) {
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(hConsole, mode);
    }
#endif
}

// ============== Console Color ==============

void platformSetConsoleColor(int colorCode) {
#ifdef _WIN32
    // Use VT (Virtual Terminal) sequences for Windows 10+
    // For older Windows, use SetConsoleTextAttribute
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) return;

    // Enable VT processing once (cached)
    static bool vtEnabled = false;
    if (!vtEnabled) {
        DWORD mode = 0;
        if (GetConsoleMode(hConsole, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            vtEnabled = (SetConsoleMode(hConsole, mode) != 0);
        }
    }

    if (vtEnabled) {
        // ANSI escape sequence works with VT mode enabled
        switch (colorCode) {
            case PC_RESET:  printf("\033[0m"); break;
            case PC_GREEN:  printf("\033[32m"); break;
            case PC_YELLOW: printf("\033[33m"); break;
            case PC_BLUE:   printf("\033[34m"); break;
            case PC_CYAN:   printf("\033[36m"); break;
            case PC_RED:    printf("\033[31m"); break;
            case PC_BOLD:   printf("\033[1m"); break;
        }
        fflush(stdout);
    } else {
        // Fallback: SetConsoleTextAttribute
        WORD attr = 0;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
            attr = csbi.wAttributes;
        }

        switch (colorCode) {
            case PC_GREEN:  attr = FOREGROUND_GREEN; break;
            case PC_YELLOW: attr = FOREGROUND_RED | FOREGROUND_GREEN; break;
            case PC_BLUE:   attr = FOREGROUND_BLUE; break;
            case PC_CYAN:   attr = FOREGROUND_GREEN | FOREGROUND_BLUE; break;
            case PC_RED:    attr = FOREGROUND_RED; break;
            case PC_BOLD:   attr |= FOREGROUND_INTENSITY; break;
            case PC_RESET:  attr = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; break;
        }
        SetConsoleTextAttribute(hConsole, attr);
    }
#else
    switch (colorCode) {
        case PC_RESET:  printf("\033[0m"); break;
        case PC_GREEN:  printf("\033[32m"); break;
        case PC_YELLOW: printf("\033[33m"); break;
        case PC_BLUE:   printf("\033[34m"); break;
        case PC_CYAN:   printf("\033[36m"); break;
        case PC_RED:    printf("\033[31m"); break;
        case PC_BOLD:   printf("\033[1m"); break;
    }
    fflush(stdout);
#endif
}

void platformResetConsoleColor() {
    platformSetConsoleColor(PC_RESET);
}

const char* platformDevNull() {
#ifdef _WIN32
    return "nul";
#else
    return "/dev/null";
#endif
}

// ============== RunDownloadScript ==============

int platformRunDownloadScript() {
    std::string exeDir = platformGetExecutableDir();
    std::string scriptPath;

#ifdef _WIN32
    // Try native/../scripts/download-ffmpeg.ps1 relative to exe
    scriptPath = platformPathJoin(exeDir, "..\\scripts\\download-ffmpeg.ps1");

    // Check if exists, if not try current directory
    if (GetFileAttributesA(scriptPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        scriptPath = platformPathJoin("scripts", "download-ffmpeg.ps1");
    }

    std::string cmd = "powershell -ExecutionPolicy Bypass -File \"" + scriptPath + "\"";
#else
    scriptPath = platformPathJoin(exeDir, "../scripts/download-ffmpeg.sh");

    // Check if exists, if not try current directory
    if (access(scriptPath.c_str(), F_OK) != 0) {
        scriptPath = platformPathJoin("scripts", "download-ffmpeg.sh");
    }

    // Make script executable first
    std::string chmodCmd = "chmod +x \"" + scriptPath + "\"";
    system(chmodCmd.c_str());

    std::string cmd = "bash \"" + scriptPath + "\"";
#endif

    return system(cmd.c_str());
}
