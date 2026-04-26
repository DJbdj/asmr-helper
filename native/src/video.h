#ifndef VIDEO_H
#define VIDEO_H

#include <string>

// FFmpeg 路径查找
std::string getFFmpegPath();

// 媒体信息获取（使用 ffprobe JSON 输出）
bool getVideoInfo(const std::string& video_path, int& width, int& height, double& duration);
bool getAudioInfo(const std::string& audio_path, double& duration);
bool getImageSize(const std::string& image_path, int& width, int& height);

// 视频处理
bool processVideo(const std::string& input_video, const std::string& overlay_image,
                  const std::string& output_path);
bool audioToVideo(const std::string& input_audio, const std::string& overlay_image,
                  const std::string& output_path, int output_width = 1920, int output_height = 1080);

// 下载 FFmpeg
void downloadFFmpeg();

// 图片选择菜单
std::string selectImage();

// 主菜单
void showMainMenu();
void showSearchMethodMenu();

#endif // VIDEO_H
