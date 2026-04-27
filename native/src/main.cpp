#include <iostream>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <curl/curl.h>
#endif

#include "ui.h"
#include "video.h"
#include "platform.h"

// 程序目录
std::string g_program_dir;

int main(int argc, char* argv[]) {
    // 切换到程序目录
    std::string exeDir = platformGetExecutableDir();
    if (!exeDir.empty()) {
        g_program_dir = exeDir;
        platformChdir(exeDir);
    }

    // 初始化
    platformInitConsole();
#ifdef _WIN32
    // Windows: WinHTTP 不需要全局初始化
    (void) argc;
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    initReadline();

    bool running = true;
    std::string input_file, output_file, image_path;

    while (running) {
        showMainMenu();
        std::string choice = getUserInput("请选择 [1-5]: ");

        if (choice == "1") {
            // 视频替换画面
            clearScreen();
            printHeader("视频替换画面");

#ifndef USE_FFMPEG_STATIC
            std::string ffmpeg_path = getFFmpegPath();
            if (ffmpeg_path.empty()) {
                std::cout << COLOR_YELLOW << "  警告：未找到 FFmpeg，请先下载" << COLOR_RESET << std::endl;
                getUserInput("按回车键继续...");
                continue;
            }
#else
            // FFmpeg is embedded, no path check needed
#endif

            // 输入视频
            std::cout << COLOR_CYAN << "  步骤 1: 选择输入视频" << COLOR_RESET << std::endl;
            input_file = getPathInput("视频文件路径（或输入 'q' 返回）: ");
            if (input_file == "q") continue;

            if (!fileExists(input_file)) {
                std::cout << COLOR_RED << "  文件不存在" << COLOR_RESET << std::endl;
                getUserInput("按回车键继续...");
                continue;
            }
            std::cout << COLOR_GREEN << "  视频文件存在" << COLOR_RESET << std::endl;

            // 选择图片
            std::cout << std::endl << COLOR_CYAN << "  步骤 2: 选择覆盖图片" << COLOR_RESET << std::endl;
            image_path = selectImage();
            if (image_path.empty()) continue;

            // 输出路径
            std::cout << std::endl << COLOR_CYAN << "  步骤 3: 设置输出文件" << COLOR_RESET << std::endl;
            output_file = getPathInput("输出文件路径（默认 output.mp4）: ");
            if (output_file.empty()) output_file = "output.mp4";

            // 确认
            std::cout << std::endl << "  输入视频：" << input_file << std::endl;
            std::cout << "  覆盖图片：" << image_path << std::endl;
            std::cout << "  输出文件：" << output_file << std::endl;

            std::string confirm = getUserInput("确认处理？[y/n]: ");
            if (confirm == "y" || confirm == "Y") {
                if (processVideo(input_file, image_path, output_file)) {
                    std::cout << COLOR_GREEN << COLOR_BOLD << "  处理完成！" << COLOR_RESET << std::endl;
                    std::cout << "  输出：" << output_file << std::endl;
                } else {
                    std::cout << COLOR_RED << "  处理失败" << COLOR_RESET << std::endl;
                }
            }

            getUserInput("按回车键继续...");

        } else if (choice == "2") {
            // 音频生成视频
            clearScreen();
            printHeader("音频生成视频");

#ifndef USE_FFMPEG_STATIC
            std::string ffmpeg_path = getFFmpegPath();
            if (ffmpeg_path.empty()) {
                std::cout << COLOR_YELLOW << "  警告：未找到 FFmpeg，请先下载" << COLOR_RESET << std::endl;
                getUserInput("按回车键继续...");
                continue;
            }
#else
            // FFmpeg is embedded, no path check needed
#endif

            // 输入音频
            std::cout << COLOR_CYAN << "  步骤 1: 选择输入音频" << COLOR_RESET << std::endl;
            std::cout << "  支持格式：mp3, wav, flac, aac, m4a 等" << std::endl;
            input_file = getPathInput("音频文件路径（或输入 'q' 返回）: ");
            if (input_file == "q") continue;

            if (!fileExists(input_file)) {
                std::cout << COLOR_RED << "  文件不存在" << COLOR_RESET << std::endl;
                getUserInput("按回车键继续...");
                continue;
            }
            std::cout << COLOR_GREEN << "  音频文件存在" << COLOR_RESET << std::endl;

            // 选择图片
            std::cout << std::endl << COLOR_CYAN << "  步骤 2: 选择视频背景图片" << COLOR_RESET << std::endl;
            image_path = selectImage();
            if (image_path.empty()) continue;

            // 视频尺寸
            std::cout << std::endl << COLOR_CYAN << "  步骤 3: 设置视频参数" << COLOR_RESET << std::endl;
            std::string size_input = getUserInput("视频尺寸（默认 1920x1080，可输入 1280x720 等）: ");

            int width = 1920, height = 1080;
            if (!size_input.empty()) {
                size_t pos = size_input.find('x');
                if (pos != std::string::npos) {
                    width = std::stoi(size_input.substr(0, pos));
                    height = std::stoi(size_input.substr(pos + 1));
                }
            }

            // 输出路径
            output_file = getUserInput("输出文件路径（默认 output.mp4）: ");
            if (output_file.empty()) output_file = "output.mp4";

            // 确认
            std::cout << std::endl << "  输入音频：" << input_file << std::endl;
            std::cout << "  背景图片：" << image_path << std::endl;
            std::cout << "  视频尺寸：" << width << "x" << height << std::endl;
            std::cout << "  输出文件：" << output_file << std::endl;

            std::string confirm = getUserInput("确认生成？[y/n]: ");
            if (confirm == "y" || confirm == "Y") {
                if (audioToVideo(input_file, image_path, output_file, width, height)) {
                    std::cout << COLOR_GREEN << COLOR_BOLD << "  视频生成完成！" << COLOR_RESET << std::endl;
                    std::cout << "  输出：" << output_file << std::endl;
                } else {
                    std::cout << COLOR_RED << "  生成失败" << COLOR_RESET << std::endl;
                }
            }

            getUserInput("按回车键继续...");

        } else if (choice == "3") {
            downloadFFmpeg();

        } else if (choice == "4") {
            clearScreen();
            printHeader("帮助信息");

            std::cout << "  功能说明：" << std::endl;
            std::cout << "  [1] 视频替换画面 - 将视频画面替换为图片，保留原音频" << std::endl;
            std::cout << "  [2] 音频生成视频 - 用图片作为背景，生成带有音频的视频" << std::endl;
            std::cout << std::endl;
            std::cout << "  图片来源：" << std::endl;
            std::cout << "  • Pexels 高质量壁纸（需英文关键词）" << std::endl;
            std::cout << "  • 图片 URL" << std::endl;
            std::cout << "  • 本地图片文件" << std::endl;
            std::cout << std::endl;
            std::cout << "  支持格式：" << std::endl;
            std::cout << "  视频：mp4, mkv, avi, mov, webm" << std::endl;
            std::cout << "  音频：mp3, wav, flac, aac, m4a" << std::endl;
            std::cout << "  图片：jpg, png, bmp, gif" << std::endl;

            getUserInput("按回车键返回...");

        } else if (choice == "5" || choice == "q") {
            running = false;
        }
    }

#ifndef _WIN32
    curl_global_cleanup();
#endif

    clearScreen();
    std::cout << COLOR_CYAN << "  感谢使用 ASMR Helper!" << COLOR_RESET << std::endl;
    std::cout << std::endl;

    return 0;
}
