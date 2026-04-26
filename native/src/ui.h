#ifndef UI_H
#define UI_H

#include <string>
#include <vector>

// ANSI 颜色代码
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RED     "\033[31m"
#define COLOR_BOLD    "\033[1m"

// 屏幕与 UI
void clearScreen();
void printHeader(const std::string& title);
void printMenu(const std::vector<std::string>& items);

// Readline 输入
void initReadline();
std::string getUserInput(const std::string& prompt);
std::string getPathInput(const std::string& prompt);

// 文件检查
bool fileExists(const std::string& path);

// 临时文件管理（使用 /tmp 目录）
class TempFileManager {
public:
    static TempFileManager& instance();
    std::string createTempFile(const std::string& suffix = ".jpg");
    void cleanup();
    ~TempFileManager();
private:
    TempFileManager();
    std::vector<std::string> temp_files_;
};

#endif // UI_H
