#ifndef FFMPEG_ENGINE_H
#define FFMPEG_ENGINE_H

#include <string>
#include <functional>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

using ProgressCallback = std::function<void(double current, double total)>;

// Media info
bool ffmpegGetVideoInfo(const std::string& path, int& w, int& h, double& dur);
bool ffmpegGetAudioInfo(const std::string& path, double& dur);
bool ffmpegGetImageSize(const std::string& path, int& w, int& h);

// Processing
bool ffmpegProcessVideo(const std::string& video, const std::string& image,
                         const std::string& output, ProgressCallback cb);
bool ffmpegAudioToVideo(const std::string& audio, const std::string& image,
                         const std::string& output, int w, int h, ProgressCallback cb);

#endif // FFMPEG_ENGINE_H
