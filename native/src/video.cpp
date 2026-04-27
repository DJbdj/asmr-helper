#include "video.h"
#include "ui.h"
#include "http.h"
#include "platform.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef USE_FFMPEG_STATIC
    #include "ffmpeg_engine.h"
#endif

#ifdef _WIN32
    #define popen  _popen
    #define pclose _pclose
#endif

extern std::string g_program_dir;

// ============== FFmpeg 路径 ==============

std::string getFFmpegPath() {
#ifdef USE_FFMPEG_STATIC
    return "";  // FFmpeg is embedded, no external binary needed
#else
    std::string localFFmpeg = platformPathJoin(g_program_dir, platformPathJoin("ffmpeg", platformGetBinaryName("ffmpeg")));
    if (fileExists(localFFmpeg)) {
        return localFFmpeg;
    }

    std::string found = platformFindCommand("ffmpeg");
    if (!found.empty()) {
        return found;
    }

    return "";
#endif
}

#ifndef USE_FFMPEG_STATIC
// ============== ffprobe JSON 解析工具 ==============

// 从 JSON 字符串中提取值（处理字符串类型的值）
static std::string jsonExtractValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos + search.length());
    if (pos == std::string::npos) return "";
    pos++;
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    if (pos >= json.length()) return "";

    if (json[pos] == '"') {
        size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - pos - 1);
    }

    // 数字值
    size_t end = pos;
    while (end < json.length() && json[end] != ',' && json[end] != '}' && json[end] != '\n' && json[end] != ' ') {
        end++;
    }
    return json.substr(pos, end - pos);
}

// 运行 ffprobe 并返回 JSON 输出
static std::string runFFprobe(const std::string& file_path, const std::string& args) {
    std::string ffprobe = platformGetBinaryName("ffprobe");
    std::string cmd = "\"" + ffprobe + "\" -v quiet -print_format json " + args + " \"" + file_path + "\" 2>" + platformDevNull();
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    return result;
}

#endif // USE_FFMPEG_STATIC (end of ffprobe JSON parsing section)

// ============== 媒体信息获取 ==============

bool getVideoInfo(const std::string& video_path, int& width, int& height, double& duration) {
#ifdef USE_FFMPEG_STATIC
    return ffmpegGetVideoInfo(video_path, width, height, duration);
#else
    std::string json = runFFprobe(video_path, "-show_streams -show_format");
    if (json.empty()) return false;

    // 找到第一个视频流（在 streams 数组中）
    size_t streams_pos = json.find("\"streams\"");
    if (streams_pos == std::string::npos) return false;

    size_t array_start = json.find('[', streams_pos);
    if (array_start == std::string::npos) return false;

    size_t stream_start = json.find('{', array_start);
    if (stream_start == std::string::npos) return false;

    // 找到视频流的结束
    int brace = 1;
    size_t stream_end = stream_start + 1;
    while (stream_end < json.length() && brace > 0) {
        if (json[stream_end] == '{') brace++;
        else if (json[stream_end] == '}') brace--;
        stream_end++;
    }

    std::string stream_json = json.substr(stream_start, stream_end - stream_start);

    std::string w = jsonExtractValue(stream_json, "width");
    std::string h = jsonExtractValue(stream_json, "height");
    std::string dur = jsonExtractValue(stream_json, "duration");

    // 如果流中没有 duration，从 format 中获取
    if (dur.empty()) {
        dur = jsonExtractValue(json, "duration");
    }

    if (w.empty() || h.empty() || dur.empty()) return false;

    width = std::stoi(w);
    height = std::stoi(h);
    duration = std::stod(dur);
    return true;
#endif
}

bool getAudioInfo(const std::string& audio_path, double& duration) {
#ifdef USE_FFMPEG_STATIC
    return ffmpegGetAudioInfo(audio_path, duration);
#else
    std::string json = runFFprobe(audio_path, "-show_format");
    if (json.empty()) return false;

    std::string dur = jsonExtractValue(json, "duration");
    if (dur.empty()) return false;

    duration = std::stod(dur);
    return true;
#endif
}

bool getImageSize(const std::string& image_path, int& width, int& height) {
#ifdef USE_FFMPEG_STATIC
    return ffmpegGetImageSize(image_path, width, height);
#else
    std::string json = runFFprobe(image_path, "-show_streams");
    if (json.empty()) return false;

    // 找到第一个视频流（图片在 ffprobe 中被当作视频流）
    size_t streams_pos = json.find("\"streams\"");
    if (streams_pos == std::string::npos) return false;

    size_t array_start = json.find('[', streams_pos);
    if (array_start == std::string::npos) return false;

    size_t stream_start = json.find('{', array_start);
    if (stream_start == std::string::npos) return false;

    int brace = 1;
    size_t stream_end = stream_start + 1;
    while (stream_end < json.length() && brace > 0) {
        if (json[stream_end] == '{') brace++;
        else if (json[stream_end] == '}') brace--;
        stream_end++;
    }

    std::string stream_json = json.substr(stream_start, stream_end - stream_start);

    std::string w = jsonExtractValue(stream_json, "width");
    std::string h = jsonExtractValue(stream_json, "height");

    if (w.empty() || h.empty()) return false;

    width = std::stoi(w);
    height = std::stoi(h);
    return true;
#endif
}

// ============== 进度条 ==============

// 解析 time=HH:MM:SS.xx 为秒数
static double parseTimeStr(const std::string& time_str) {
    int h = 0, m = 0;
    double s = 0;
    if (sscanf(time_str.c_str(), "%d:%d:%lf", &h, &m, &s) == 3) {
        return h * 3600 + m * 60 + s;
    }
    return -1;
}

// 格式化秒数为 HH:MM:SS
static std::string formatTime(double seconds) {
    int h = (int)seconds / 3600;
    int m = ((int)seconds % 3600) / 60;
    int s = (int)seconds % 60;
    std::ostringstream oss;
    if (h > 0) {
        oss << std::setfill('0') << std::setw(2) << h << ":";
    }
    oss << std::setfill('0') << std::setw(2) << m << ":"
        << std::setfill('0') << std::setw(2) << s;
    return oss.str();
}

// 显示进度条
static void showProgressBar(double current, double total) {
    if (total <= 0) return;

    double pct = (current / total) * 100.0;
    int filled = (int)(pct / 5);  // 20 个槽位
    if (filled > 20) filled = 20;
    if (filled < 0) filled = 0;

    std::cout << "\r  [";
    for (int i = 0; i < 20; i++) {
        if (i < filled) std::cout << COLOR_GREEN << "\xe2\x96\x88" COLOR_RESET;
        else std::cout << COLOR_BOLD << "\xe2\x96\x91" COLOR_RESET;
    }
    std::cout << "] " << std::fixed << std::setprecision(1) << pct << "% "
              << "(" << formatTime(current) << " / " << formatTime(total) << ")"
              << std::flush;
}

// ============== 视频处理 ==============

bool processVideo(const std::string& input_video, const std::string& overlay_image,
                  const std::string& output_path) {
    int width, height;
    double duration;

    if (!getVideoInfo(input_video, width, height, duration)) {
        std::cerr << COLOR_RED << "  无法获取视频信息" << COLOR_RESET << std::endl;
        return false;
    }

    std::cout << "  视频尺寸：" << width << "x" << height << std::endl;
    std::cout << "  视频时长：" << duration << " 秒" << std::endl;

#ifdef USE_FFMPEG_STATIC
    std::cout << COLOR_YELLOW << "  正在处理（FFmpeg 内嵌模式）..." << COLOR_RESET << std::endl;
    return ffmpegProcessVideo(input_video, overlay_image, output_path,
        [](double cur, double tot) { showProgressBar(cur, tot); });
#else
    std::string ffmpeg_path = getFFmpegPath();
    if (ffmpeg_path.empty()) {
        std::cerr << COLOR_RED << "  错误：未找到 FFmpeg" << COLOR_RESET << std::endl;
        std::cerr << "  请运行程序目录下的 scripts/download-ffmpeg.sh" << std::endl;
        return false;
    }

    std::cout << "  FFmpeg：" << ffmpeg_path << std::endl;

    // FFmpeg 命令：替换视频画面，保留音频
    // 使用 -nostats -progress pipe:2 来获取精确进度
    std::string filter = "[1:v]scale=" + std::to_string(width) + ":" + std::to_string(height)
        + ":force_original_aspect_ratio=decrease,pad=" + std::to_string(width) + ":"
        + std::to_string(height) + ":(ow-iw)/2:(oh-ih)/2[img];[0:v][img]overlay=0:0[outv]";

    std::string cmd = "\"" + ffmpeg_path + "\" -y -i \"" + input_video + "\" -i \"" + overlay_image + "\" "
        "-filter_complex \"" + filter + "\" "
        "-map \"[outv]\" -map 0:a? -c:v libx264 -preset medium -crf 23 -c:a copy "
        "-t " + std::to_string(duration) + " -nostats -progress pipe:2 \"" + output_path + "\" 2>&1";

    std::cout << COLOR_YELLOW << "  正在处理..." << COLOR_RESET << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << COLOR_RED << "  无法执行 FFmpeg" << COLOR_RESET << std::endl;
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);

        // 查找 time= 行
        if (line.find("time=") == 0) {
            std::string time_val = line.substr(5);  // 跳过 "time="
            size_t nl = time_val.find('\n');
            if (nl != std::string::npos) time_val = time_val.substr(0, nl);

            double current = parseTimeStr(time_val);
            if (current >= 0) {
                showProgressBar(current, duration);
            }
        }
    }

    // 输出最终进度条（100%）
    showProgressBar(duration, duration);
    std::cout << std::endl;

    int ret = pclose(pipe);

    if (ret != 0) {
        std::cerr << COLOR_RED << "  FFmpeg 处理失败" << COLOR_RESET << std::endl;
        return false;
    }

    return true;
#endif
}

bool audioToVideo(const std::string& input_audio, const std::string& overlay_image,
                  const std::string& output_path, int output_width, int output_height) {
    double duration;
    if (!getAudioInfo(input_audio, duration)) {
        std::cerr << COLOR_RED << "  无法获取音频信息" << COLOR_RESET << std::endl;
        return false;
    }

    std::cout << "  音频时长：" << duration << " 秒" << std::endl;

    int img_w, img_h;
    if (getImageSize(overlay_image, img_w, img_h)) {
        std::cout << "  图片尺寸：" << img_w << "x" << img_h << std::endl;
    }

#ifdef USE_FFMPEG_STATIC
    std::cout << COLOR_YELLOW << "  正在生成视频（FFmpeg 内嵌模式）..." << COLOR_RESET << std::endl;
    return ffmpegAudioToVideo(input_audio, overlay_image, output_path,
        output_width, output_height,
        [](double cur, double tot) { showProgressBar(cur, tot); });
#else
    std::string ffmpeg_path = getFFmpegPath();
    if (ffmpeg_path.empty()) {
        std::cerr << COLOR_RED << "  错误：未找到 FFmpeg" << COLOR_RESET << std::endl;
        return false;
    }

    std::cout << "  FFmpeg：" << ffmpeg_path << std::endl;

    std::string filter = "[0:v]scale=" + std::to_string(output_width) + ":" + std::to_string(output_height)
        + ":force_original_aspect_ratio=decrease,pad=" + std::to_string(output_width) + ":"
        + std::to_string(output_height) + ":(ow-iw)/2:(oh-ih)/2:black[outv]";

    std::string cmd = "\"" + ffmpeg_path + "\" -y -loop 1 -i \"" + overlay_image + "\" -i \"" + input_audio + "\" "
        "-filter_complex \"" + filter + "\" "
        "-map \"[outv]\" -map 1:a -c:v libx264 -preset medium -crf 23 -c:a aac -b:a 192k "
        "-t " + std::to_string(duration) + " -pix_fmt yuv420p -nostats -progress pipe:2 \""
        + output_path + "\" 2>&1";

    std::cout << COLOR_YELLOW << "  正在生成视频..." << COLOR_RESET << std::endl;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << COLOR_RED << "  无法执行 FFmpeg" << COLOR_RESET << std::endl;
        return false;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);

        if (line.find("time=") == 0) {
            std::string time_val = line.substr(5);
            size_t nl = time_val.find('\n');
            if (nl != std::string::npos) time_val = time_val.substr(0, nl);

            double current = parseTimeStr(time_val);
            if (current >= 0) {
                showProgressBar(current, duration);
            }
        }
    }

    showProgressBar(duration, duration);
    std::cout << std::endl;

    int ret = pclose(pipe);

    if (ret != 0) {
        std::cerr << COLOR_RED << "  FFmpeg 处理失败" << COLOR_RESET << std::endl;
        return false;
    }

    return true;
#endif
}

// ============== 菜单函数 ==============

void showMainMenu() {
    clearScreen();
    printHeader("ASMR Helper - 视频源替换工具");

    std::cout << COLOR_CYAN << "  欢迎使用 ASMR Helper!" << COLOR_RESET << std::endl;
    std::cout << "  本工具可以将视频画面替换为图片，或从音频生成视频。" << std::endl;
    std::cout << std::endl;

    std::vector<std::string> options = {
        "视频替换画面（保留音频）",
        "音频生成视频（新功能）",
        "下载 FFmpeg",
        "查看帮助",
        "退出程序"
    };
    printMenu(options);
}

void showSearchMethodMenu() {
    printHeader("选择图片获取方式");
    std::vector<std::string> options = {
        "从 Pexels 搜索壁纸",
        "使用图片 URL",
        "使用本地图片文件",
        "返回"
    };
    printMenu(options);
}

void downloadFFmpeg() {
    clearScreen();
    printHeader("下载 FFmpeg");

#ifdef USE_FFMPEG_STATIC
    std::cout << COLOR_GREEN << "  FFmpeg 已内嵌到程序中，无需下载！" << COLOR_RESET << std::endl;
#else
    int ret = platformRunDownloadScript();

    if (ret == 0) {
        std::cout << COLOR_GREEN << "  FFmpeg 下载完成！" << COLOR_RESET << std::endl;
    } else {
        std::cerr << COLOR_RED << "  下载失败" << COLOR_RESET << std::endl;
    }
#endif

    getUserInput("按回车键继续...");
}

// 图片选择流程 - 返回图片路径，临时文件由 TempFileManager 管理
std::string selectImage() {
    while (true) {
        showSearchMethodMenu();
        std::string choice = getUserInput("请选择 [1-4]: ");

        if (choice == "1") {
            // Pexels 搜索
            std::string query = getUserInput("请输入壁纸关键词（英文，如 nature, city, space）: ");
            if (query.empty()) continue;

            std::cout << COLOR_YELLOW << "  正在搜索..." << COLOR_RESET << std::endl;
            std::string url = searchImagePexels(query);

            if (!url.empty()) {
                std::cout << COLOR_GREEN << "  找到图片：" << url << COLOR_RESET << std::endl;

                std::string temp_path = TempFileManager::instance().createTempFile(".jpg");
                std::cout << "  正在下载..." << std::endl;

                if (downloadImage(url, temp_path)) {
                    std::cout << COLOR_GREEN << "  图片已下载" << COLOR_RESET << std::endl;
                    return temp_path;
                } else {
                    std::cout << COLOR_RED << "  下载失败" << COLOR_RESET << std::endl;
                }
            } else {
                std::cout << COLOR_RED << "  未找到图片，请尝试其他关键词" << COLOR_RESET << std::endl;
            }

        } else if (choice == "2") {
            // URL
            std::string url = getUserInput("请输入图片 URL: ");
            if (url.empty()) continue;

            std::string temp_path = TempFileManager::instance().createTempFile(".jpg");
            std::cout << "  正在下载..." << std::endl;

            if (downloadImage(url, temp_path)) {
                std::cout << COLOR_GREEN << "  图片已下载" << COLOR_RESET << std::endl;
                return temp_path;
            } else {
                std::cout << COLOR_RED << "  下载失败" << COLOR_RESET << std::endl;
            }

        } else if (choice == "3") {
            // 本地文件
            std::string path = getPathInput("请输入图片文件路径: ");

            if (fileExists(path)) {
                std::cout << COLOR_GREEN << "  图片文件存在" << COLOR_RESET << std::endl;
                return path;
            } else {
                std::cout << COLOR_RED << "  文件不存在" << COLOR_RESET << std::endl;
            }

        } else if (choice == "4") {
            return "";
        }
    }
}
