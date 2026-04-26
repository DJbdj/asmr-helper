#include "ffmpeg_engine.h"
#include <cstdio>
#include <cstring>
#include <iostream>

// ============== 媒体信息获取 ==============

static bool openInputAndFindStreams(const std::string& path, AVFormatContext*& fmtCtx) {
    fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, path.c_str(), nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        avformat_close_input(&fmtCtx);
        return false;
    }
    return true;
}

static bool findVideoStream(AVFormatContext* fmtCtx, int& idx) {
    idx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            idx = (int)i;
            return true;
        }
    }
    return false;
}

static bool findAudioStream(AVFormatContext* fmtCtx, int& idx) {
    idx = -1;
    for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
        if (fmtCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            idx = (int)i;
            return true;
        }
    }
    return false;
}

bool ffmpegGetVideoInfo(const std::string& path, int& w, int& h, double& dur) {
    AVFormatContext* fmtCtx = nullptr;
    if (!openInputAndFindStreams(path, fmtCtx)) return false;

    int vidx;
    if (!findVideoStream(fmtCtx, vidx)) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecParameters* par = fmtCtx->streams[vidx]->codecpar;
    w = par->width;
    h = par->height;

    // Get duration from stream or format context
    dur = 0;
    if (fmtCtx->streams[vidx]->duration > 0) {
        dur = (double)fmtCtx->streams[vidx]->duration *
              av_q2d(fmtCtx->streams[vidx]->time_base);
    } else if (fmtCtx->duration > 0) {
        dur = (double)fmtCtx->duration / (double)AV_TIME_BASE;
    }

    avformat_close_input(&fmtCtx);
    return dur > 0 && w > 0 && h > 0;
}

bool ffmpegGetAudioInfo(const std::string& path, double& dur) {
    AVFormatContext* fmtCtx = nullptr;
    if (!openInputAndFindStreams(path, fmtCtx)) return false;

    dur = 0;
    if (fmtCtx->duration > 0) {
        dur = (double)fmtCtx->duration / (double)AV_TIME_BASE;
    }

    // Try to find audio stream to verify it's an audio file
    int aidx;
    if (!findAudioStream(fmtCtx, aidx)) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    // If stream duration available, use it
    if (fmtCtx->streams[aidx]->duration > 0) {
        dur = (double)fmtCtx->streams[aidx]->duration *
              av_q2d(fmtCtx->streams[aidx]->time_base);
    }

    avformat_close_input(&fmtCtx);
    return dur > 0;
}

bool ffmpegGetImageSize(const std::string& path, int& w, int& h) {
    AVFormatContext* fmtCtx = nullptr;
    if (!openInputAndFindStreams(path, fmtCtx)) return false;

    int vidx;
    if (!findVideoStream(fmtCtx, vidx)) {
        avformat_close_input(&fmtCtx);
        return false;
    }

    AVCodecParameters* par = fmtCtx->streams[vidx]->codecpar;
    w = par->width;
    h = par->height;

    avformat_close_input(&fmtCtx);
    return w > 0 && h > 0;
}

// ============== 工具函数 ==============

// 打开解码器
static AVCodecContext* openDecoder(AVCodecParameters* codecpar) {
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) return nullptr;

    AVCodecContext* ctx = avcodec_alloc_context3(codec);
    if (!ctx) return nullptr;

    if (avcodec_parameters_to_context(ctx, codecpar) < 0 ||
        avcodec_open2(ctx, codec, nullptr) < 0) {
        avcodec_free_context(&ctx);
        return nullptr;
    }
    return ctx;
}

// 缩放图片到目标尺寸
static AVFrame* scaleImageFrame(AVFrame* src, int targetW, int targetH) {
    SwsContext* sws = sws_getContext(
        src->width, src->height, (AVPixelFormat)src->format,
        targetW, targetH, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!sws) return nullptr;

    AVFrame* dst = av_frame_alloc();
    if (!dst) { sws_freeContext(sws); return nullptr; }

    dst->format = AV_PIX_FMT_YUV420P;
    dst->width = targetW;
    dst->height = targetH;
    if (av_frame_get_buffer(dst, 0) < 0) {
        av_frame_free(&dst);
        sws_freeContext(sws);
        return nullptr;
    }

    sws_scale(sws, src->data, src->linesize, 0, src->height,
              dst->data, dst->linesize);
    sws_freeContext(sws);
    return dst;
}

// 解码图片到 YUV420P frame
static AVFrame* decodeImageToFrame(const std::string& path, int targetW, int targetH) {
    AVFormatContext* fmtCtx = nullptr;
    if (!openInputAndFindStreams(path, fmtCtx)) return nullptr;

    int vidx;
    if (!findVideoStream(fmtCtx, vidx)) {
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    AVCodecContext* decCtx = openDecoder(fmtCtx->streams[vidx]->codecpar);
    if (!decCtx) {
        avformat_close_input(&fmtCtx);
        return nullptr;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* result = nullptr;

    if (av_read_frame(fmtCtx, pkt) >= 0 && pkt->stream_index == vidx) {
        if (avcodec_send_packet(decCtx, pkt) == 0 &&
            avcodec_receive_frame(decCtx, frame) == 0) {
            result = scaleImageFrame(frame, targetW, targetH);
        }
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&decCtx);
    avformat_close_input(&fmtCtx);
    return result;
}

// ============== 视频处理（画面替换） ==============

bool ffmpegProcessVideo(const std::string& video, const std::string& image,
                         const std::string& output, ProgressCallback cb) {
    // 1. Open input video
    AVFormatContext* vidFmtCtx = nullptr;
    if (!openInputAndFindStreams(video, vidFmtCtx)) {
        std::cerr << "Cannot open video: " << video << std::endl;
        return false;
    }

    int vidx, aidx = -1;
    if (!findVideoStream(vidFmtCtx, vidx)) {
        avformat_close_input(&vidFmtCtx);
        return false;
    }
    findAudioStream(vidFmtCtx, aidx);

    AVCodecContext* vidDecCtx = openDecoder(vidFmtCtx->streams[vidx]->codecpar);
    if (!vidDecCtx) {
        avformat_close_input(&vidFmtCtx);
        return false;
    }

    int srcW = vidDecCtx->width;
    int srcH = vidDecCtx->height;
    double duration = 0;
    if (vidFmtCtx->streams[vidx]->duration > 0) {
        duration = (double)vidFmtCtx->streams[vidx]->duration *
                   av_q2d(vidFmtCtx->streams[vidx]->time_base);
    } else if (vidFmtCtx->duration > 0) {
        duration = (double)vidFmtCtx->duration / (double)AV_TIME_BASE;
    }

    // 2. Decode image to frame
    AVFrame* imgFrame = decodeImageToFrame(image, srcW, srcH);
    if (!imgFrame) {
        std::cerr << "Cannot decode image: " << image << std::endl;
        avcodec_free_context(&vidDecCtx);
        avformat_close_input(&vidFmtCtx);
        return false;
    }

    // 3. Setup filter graph: overlay image on video
    // [in0] = video frames, [in1] = static image, output = overlay
    AVFilterGraph* filterGraph = avfilter_graph_alloc();

    char args0[512];
    snprintf(args0, sizeof(args0),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        vidDecCtx->width, vidDecCtx->height, vidDecCtx->pix_fmt,
        vidFmtCtx->streams[vidx]->time_base.num,
        vidFmtCtx->streams[vidx]->time_base.den,
        vidDecCtx->sample_aspect_ratio.num, vidDecCtx->sample_aspect_ratio.den);

    AVFilterContext* src0Ctx = nullptr;
    avfilter_graph_create_filter(&src0Ctx, avfilter_get_by_name("buffer"),
        "in0", args0, nullptr, filterGraph);

    // Image as second input - use the scaled image dimensions
    char args1[256];
    snprintf(args1, sizeof(args1),
        "video_size=%dx%d:pix_fmt=%d:time_base=1/30:pixel_aspect=1/1",
        imgFrame->width, imgFrame->height, imgFrame->format);

    AVFilterContext* src1Ctx = nullptr;
    avfilter_graph_create_filter(&src1Ctx, avfilter_get_by_name("buffer"),
        "in1", args1, nullptr, filterGraph);

    AVFilterContext* sinkCtx = nullptr;
    avfilter_graph_create_filter(&sinkCtx, avfilter_get_by_name("buffersink"),
        "out", nullptr, nullptr, filterGraph);

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    av_opt_set_int_list(sinkCtx, "pix_fmts", pix_fmts,
        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();

    outputs->name = av_strdup("out");
    outputs->filter_ctx = sinkCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("in0");
    inputs->filter_ctx = src0Ctx;
    inputs->pad_idx = 0;
    inputs->next = avfilter_inout_alloc();
    inputs->next->name = av_strdup("in1");
    inputs->next->filter_ctx = src1Ctx;
    inputs->next->pad_idx = 0;
    inputs->next->next = nullptr;

    // We use overlay filter - but first scale the image
    std::string filterDesc =
        "[in1]scale=" + std::to_string(srcW) + ":" + std::to_string(srcH) +
        ":force_original_aspect_ratio=decrease,"
        "pad=" + std::to_string(srcW) + ":" + std::to_string(srcH) +
        ":(ow-iw)/2:(oh-ih)/2[img];"
        "[in0][img]overlay=0:0[out]";

    avfilter_graph_parse_ptr(filterGraph, filterDesc.c_str(), &inputs, &outputs, nullptr);
    avfilter_graph_config(filterGraph, nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    // 4. Setup H.264 encoder
    const AVCodec* encCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encCodec) {
        std::cerr << "Cannot find H.264 encoder" << std::endl;
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&vidDecCtx);
        avformat_close_input(&vidFmtCtx);
        return false;
    }

    AVCodecContext* encCtx = avcodec_alloc_context3(encCodec);
    encCtx->width = srcW;
    encCtx->height = srcH;
    encCtx->time_base = vidFmtCtx->streams[vidx]->time_base;
    encCtx->framerate = vidFmtCtx->streams[vidx]->r_frame_rate;
    if (encCtx->framerate.num == 0) {
        encCtx->framerate = (AVRational){30, 1};
        encCtx->time_base = (AVRational){1, 30};
    }
    encCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    encCtx->gop_size = 12;

    AVDictionary* encOpts = nullptr;
    av_dict_set(&encOpts, "preset", "medium", 0);
    av_dict_set(&encOpts, "crf", "23", 0);

    if (avcodec_open2(encCtx, encCodec, &encOpts) < 0) {
        std::cerr << "Cannot open H.264 encoder" << std::endl;
        av_dict_free(&encOpts);
        avcodec_free_context(&encCtx);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&vidDecCtx);
        avformat_close_input(&vidFmtCtx);
        return false;
    }
    av_dict_free(&encOpts);

    // 5. Setup output file
    AVFormatContext* outFmtCtx = nullptr;
    avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr, output.c_str());
    if (!outFmtCtx) {
        avcodec_free_context(&encCtx);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&vidDecCtx);
        avformat_close_input(&vidFmtCtx);
        return false;
    }

    AVStream* outVidStream = avformat_new_stream(outFmtCtx, nullptr);
    avcodec_parameters_from_context(outVidStream->codecpar, encCtx);
    outVidStream->time_base = encCtx->time_base;

    AVStream* outAudStream = nullptr;
    if (aidx >= 0) {
        outAudStream = avformat_new_stream(outFmtCtx, nullptr);
        avcodec_parameters_copy(outAudStream->codecpar, vidFmtCtx->streams[aidx]->codecpar);
        outAudStream->time_base = vidFmtCtx->streams[aidx]->time_base;
    }

    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFmtCtx->pb, output.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Cannot open output file: " << output << std::endl;
            avformat_free_context(outFmtCtx);
            avcodec_free_context(&encCtx);
            avfilter_graph_free(&filterGraph);
            av_frame_free(&imgFrame);
            avcodec_free_context(&vidDecCtx);
            avformat_close_input(&vidFmtCtx);
            return false;
        }
    }

    avformat_write_header(outFmtCtx, nullptr);

    // 6. Push image frame into filter once
    av_buffersrc_add_frame_flags(src1Ctx, imgFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

    // 7. Main processing loop
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* filtFrame = av_frame_alloc();
    int64_t frameCount = 0;
    int64_t maxFrames = (int64_t)(duration * av_q2d(encCtx->framerate));
    bool videoFinished = false;

    while (!videoFinished && frameCount < maxFrames) {
        int ret = av_read_frame(vidFmtCtx, pkt);
        if (ret < 0) {
            // EOF - flush decoder
            avcodec_send_packet(vidDecCtx, nullptr);
            videoFinished = true;
        }

        if (!videoFinished && pkt->stream_index == (unsigned)vidx) {
            avcodec_send_packet(vidDecCtx, pkt);
        } else if (pkt->stream_index == (unsigned)aidx && outAudStream) {
            // Forward audio packet directly
            AVStream* inAud = vidFmtCtx->streams[aidx];
            av_packet_rescale_ts(pkt, inAud->time_base, outAudStream->time_base);
            pkt->stream_index = outAudStream->index;
            av_interleaved_write_frame(outFmtCtx, pkt);
        }
        av_packet_unref(pkt);

        // Receive decoded frames
        while (avcodec_receive_frame(vidDecCtx, frame) == 0) {
            // Push to filter
            av_buffersrc_add_frame_flags(src0Ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);

            // Pull filtered frames
            while (av_buffersink_get_frame(sinkCtx, filtFrame) == 0) {
                filtFrame->pts = frameCount++;

                // Encode
                avcodec_send_frame(encCtx, filtFrame);

                AVPacket* encPkt = av_packet_alloc();
                while (avcodec_receive_packet(encCtx, encPkt) == 0) {
                    av_packet_rescale_ts(encPkt, encCtx->time_base, outVidStream->time_base);
                    encPkt->stream_index = outVidStream->index;
                    av_interleaved_write_frame(outFmtCtx, encPkt);
                }
                av_packet_free(&encPkt);
                av_frame_unref(filtFrame);
            }

            // Progress callback
            if (cb && duration > 0) {
                double current = (double)frameCount / av_q2d(encCtx->framerate);
                if (current > duration) current = duration;
                cb(current, duration);
            }

            av_frame_unref(frame);
        }

        if (videoFinished && frameCount >= maxFrames) break;
    }

    // Flush remaining decoder
    avcodec_send_packet(vidDecCtx, nullptr);
    while (avcodec_receive_frame(vidDecCtx, frame) == 0) {
        av_buffersrc_add_frame_flags(src0Ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        while (av_buffersink_get_frame(sinkCtx, filtFrame) == 0) {
            filtFrame->pts = frameCount++;
            avcodec_send_frame(encCtx, filtFrame);
            AVPacket* encPkt = av_packet_alloc();
            while (avcodec_receive_packet(encCtx, encPkt) == 0) {
                av_packet_rescale_ts(encPkt, encCtx->time_base, outVidStream->time_base);
                encPkt->stream_index = outVidStream->index;
                av_interleaved_write_frame(outFmtCtx, encPkt);
            }
            av_packet_free(&encPkt);
            av_frame_unref(filtFrame);
        }
        av_frame_unref(frame);
    }

    // Flush encoder
    avcodec_send_frame(encCtx, nullptr);
    AVPacket* encPkt = av_packet_alloc();
    while (avcodec_receive_packet(encCtx, encPkt) == 0) {
        av_packet_rescale_ts(encPkt, encCtx->time_base, outVidStream->time_base);
        encPkt->stream_index = outVidStream->index;
        av_interleaved_write_frame(outFmtCtx, encPkt);
    }
    av_packet_free(&encPkt);

    if (cb && duration > 0) cb(duration, duration);

    av_write_trailer(outFmtCtx);

    // Cleanup
    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&outFmtCtx->pb);
    avformat_free_context(outFmtCtx);
    avcodec_free_context(&encCtx);
    avfilter_graph_free(&filterGraph);
    av_frame_free(&filtFrame);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    av_frame_free(&imgFrame);
    avcodec_free_context(&vidDecCtx);
    avformat_close_input(&vidFmtCtx);

    return true;
}

// ============== 音频生成视频 ==============

bool ffmpegAudioToVideo(const std::string& audio, const std::string& image,
                         const std::string& output, int outW, int outH, ProgressCallback cb) {
    // 1. Open audio input
    AVFormatContext* audFmtCtx = nullptr;
    if (!openInputAndFindStreams(audio, audFmtCtx)) {
        std::cerr << "Cannot open audio: " << audio << std::endl;
        return false;
    }

    int aidx;
    if (!findAudioStream(audFmtCtx, aidx)) {
        avformat_close_input(&audFmtCtx);
        return false;
    }

    AVCodecContext* audDecCtx = openDecoder(audFmtCtx->streams[aidx]->codecpar);
    if (!audDecCtx) {
        avformat_close_input(&audFmtCtx);
        return false;
    }

    double duration = 0;
    if (audFmtCtx->streams[aidx]->duration > 0) {
        duration = (double)audFmtCtx->streams[aidx]->duration *
                   av_q2d(audFmtCtx->streams[aidx]->time_base);
    } else if (audFmtCtx->duration > 0) {
        duration = (double)audFmtCtx->duration / (double)AV_TIME_BASE;
    }

    // 2. Decode image
    AVFrame* imgFrame = decodeImageToFrame(image, outW, outH);
    if (!imgFrame) {
        std::cerr << "Cannot decode image: " << image << std::endl;
        avcodec_free_context(&audDecCtx);
        avformat_close_input(&audFmtCtx);
        return false;
    }

    // 3. Setup filter graph (scale/pad image)
    AVFilterGraph* filterGraph = avfilter_graph_alloc();

    char args[256];
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=1/30",
        imgFrame->width, imgFrame->height, imgFrame->format);

    AVFilterContext* srcCtx = nullptr;
    avfilter_graph_create_filter(&srcCtx, avfilter_get_by_name("buffer"),
        "in", args, nullptr, filterGraph);

    AVFilterContext* sinkCtx = nullptr;
    avfilter_graph_create_filter(&sinkCtx, avfilter_get_by_name("buffersink"),
        "out", nullptr, nullptr, filterGraph);

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    av_opt_set_int_list(sinkCtx, "pix_fmts", pix_fmts,
        AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);

    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();

    outputs->name = av_strdup("out");
    outputs->filter_ctx = sinkCtx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("in");
    inputs->filter_ctx = srcCtx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    char filterDesc[512];
    snprintf(filterDesc, sizeof(filterDesc),
        "[in]scale=%d:%d:force_original_aspect_ratio=decrease,"
        "pad=%d:%d:(ow-iw)/2:(oh-ih)/2:black[out]",
        outW, outH, outW, outH);

    avfilter_graph_parse_ptr(filterGraph, filterDesc, &inputs, &outputs, nullptr);
    avfilter_graph_config(filterGraph, nullptr);

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    // 4. Setup H.264 encoder
    const AVCodec* vidEncCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!vidEncCodec) {
        std::cerr << "Cannot find H.264 encoder" << std::endl;
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&audDecCtx);
        avformat_close_input(&audFmtCtx);
        return false;
    }

    AVCodecContext* vidEncCtx = avcodec_alloc_context3(vidEncCodec);
    vidEncCtx->width = outW;
    vidEncCtx->height = outH;
    vidEncCtx->time_base = (AVRational){1, 30};
    vidEncCtx->framerate = (AVRational){30, 1};
    vidEncCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    vidEncCtx->gop_size = 12;

    AVDictionary* vidOpts = nullptr;
    av_dict_set(&vidOpts, "preset", "medium", 0);
    av_dict_set(&vidOpts, "crf", "23", 0);
    avcodec_open2(vidEncCtx, vidEncCodec, &vidOpts);
    av_dict_free(&vidOpts);

    // 5. Setup AAC encoder
    const AVCodec* audEncCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audEncCodec) {
        std::cerr << "Cannot find AAC encoder" << std::endl;
        avcodec_free_context(&vidEncCtx);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&audDecCtx);
        avformat_close_input(&audFmtCtx);
        return false;
    }

    AVCodecContext* audEncCtx = avcodec_alloc_context3(audEncCodec);
    audEncCtx->sample_rate = audDecCtx->sample_rate;
#if LIBAVCODEC_VERSION_MAJOR >= 61
    // FFmpeg 7.x+: use ch_layout
    audEncCtx->ch_layout = audDecCtx->ch_layout;
#else
    audEncCtx->channel_layout = audDecCtx->channel_layout;
    audEncCtx->channels = av_get_channel_layout_nb_channels(audDecCtx->channel_layout);
#endif
    audEncCtx->sample_fmt = audEncCodec->sample_fmts[0];
    audEncCtx->bit_rate = 192000;
    avcodec_open2(audEncCtx, audEncCodec, nullptr);

    // 6. Setup output
    AVFormatContext* outFmtCtx = nullptr;
    avformat_alloc_output_context2(&outFmtCtx, nullptr, nullptr, output.c_str());
    if (!outFmtCtx) {
        avcodec_free_context(&audEncCtx);
        avcodec_free_context(&vidEncCtx);
        avfilter_graph_free(&filterGraph);
        av_frame_free(&imgFrame);
        avcodec_free_context(&audDecCtx);
        avformat_close_input(&audFmtCtx);
        return false;
    }

    AVStream* outVidStream = avformat_new_stream(outFmtCtx, nullptr);
    avcodec_parameters_from_context(outVidStream->codecpar, vidEncCtx);
    outVidStream->time_base = vidEncCtx->time_base;

    AVStream* outAudStream = avformat_new_stream(outFmtCtx, nullptr);
    avcodec_parameters_from_context(outAudStream->codecpar, audEncCtx);
    outAudStream->time_base = audEncCtx->time_base;

    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outFmtCtx->pb, output.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Cannot open output file: " << output << std::endl;
            avformat_free_context(outFmtCtx);
            avcodec_free_context(&audEncCtx);
            avcodec_free_context(&vidEncCtx);
            avfilter_graph_free(&filterGraph);
            av_frame_free(&imgFrame);
            avcodec_free_context(&audDecCtx);
            avformat_close_input(&audFmtCtx);
            return false;
        }
    }

    avformat_write_header(outFmtCtx, nullptr);

    // 7. Push image into filter
    imgFrame->pts = 0;
    av_buffersrc_add_frame_flags(srcCtx, imgFrame, AV_BUFFERSRC_FLAG_KEEP_REF);

    // 8. Get filtered frame (reference for repeated use)
    AVFrame* filtFrame = av_frame_alloc();
    av_buffersink_get_frame(sinkCtx, filtFrame);

    // 9. Main loop: decode audio -> encode audio, reuse filtered frame for video
    AVPacket* pkt = av_packet_alloc();
    AVFrame* audFrame = av_frame_alloc();
    int64_t totalAudioFrames = 0;
    double currentTime = 0;
    bool audioDone = false;

    while (!audioDone) {
        int ret = av_read_frame(audFmtCtx, pkt);
        if (ret < 0) {
            avcodec_send_packet(audDecCtx, nullptr);
            audioDone = true;
        }

        if (!audioDone && pkt->stream_index == (unsigned)aidx) {
            avcodec_send_packet(audDecCtx, pkt);
        }
        av_packet_unref(pkt);

        // Decode audio frames
        while (avcodec_receive_frame(audDecCtx, audFrame) == 0) {
            totalAudioFrames++;

            // Calculate current time from audio frame
            currentTime = (double)audFrame->pts *
                          av_q2d(audFmtCtx->streams[aidx]->time_base);

            // Encode video frames for this audio duration
            // Generate enough video frames to match audio timing
            AVFrame* vidFrame = av_frame_clone(filtFrame);
            vidFrame->pts = totalAudioFrames - 1;
            // Set video time base
            vidFrame->pts = (int64_t)(currentTime * 30);

            avcodec_send_frame(vidEncCtx, vidFrame);
            av_frame_free(&vidFrame);

            AVPacket* vidEncPkt = av_packet_alloc();
            while (avcodec_receive_packet(vidEncCtx, vidEncPkt) == 0) {
                av_packet_rescale_ts(vidEncPkt, vidEncCtx->time_base, outVidStream->time_base);
                vidEncPkt->stream_index = outVidStream->index;
                av_interleaved_write_frame(outFmtCtx, vidEncPkt);
            }
            av_packet_free(&vidEncPkt);

            // Encode audio
            avcodec_send_frame(audEncCtx, audFrame);
            AVPacket* audEncPkt = av_packet_alloc();
            while (avcodec_receive_packet(audEncCtx, audEncPkt) == 0) {
                av_packet_rescale_ts(audEncPkt, audEncCtx->time_base, outAudStream->time_base);
                audEncPkt->stream_index = outAudStream->index;
                av_interleaved_write_frame(outFmtCtx, audEncPkt);
            }
            av_packet_free(&audEncPkt);

            // Progress
            if (cb && duration > 0) {
                double cur = currentTime;
                if (cur > duration) cur = duration;
                cb(cur, duration);
            }

            av_frame_unref(audFrame);
        }
    }

    // Flush video encoder - send remaining frames to match audio duration
    int64_t remainingFrames = (int64_t)(duration * 30) - totalAudioFrames;
    for (int64_t i = 0; i < remainingFrames; i++) {
        AVFrame* vidFrame = av_frame_clone(filtFrame);
        vidFrame->pts = totalAudioFrames + i;
        avcodec_send_frame(vidEncCtx, vidFrame);
        av_frame_free(&vidFrame);
        AVPacket* vidEncPkt = av_packet_alloc();
        while (avcodec_receive_packet(vidEncCtx, vidEncPkt) == 0) {
            av_packet_rescale_ts(vidEncPkt, vidEncCtx->time_base, outVidStream->time_base);
            vidEncPkt->stream_index = outVidStream->index;
            av_interleaved_write_frame(outFmtCtx, vidEncPkt);
        }
        av_packet_free(&vidEncPkt);
    }

    // Flush encoders
    avcodec_send_frame(vidEncCtx, nullptr);
    AVPacket* vidFlushPkt = av_packet_alloc();
    while (avcodec_receive_packet(vidEncCtx, vidFlushPkt) == 0) {
        av_packet_rescale_ts(vidFlushPkt, vidEncCtx->time_base, outVidStream->time_base);
        vidFlushPkt->stream_index = outVidStream->index;
        av_interleaved_write_frame(outFmtCtx, vidFlushPkt);
    }
    av_packet_free(&vidFlushPkt);

    avcodec_send_frame(audEncCtx, nullptr);
    AVPacket* audFlushPkt = av_packet_alloc();
    while (avcodec_receive_packet(audEncCtx, audFlushPkt) == 0) {
        av_packet_rescale_ts(audFlushPkt, audEncCtx->time_base, outAudStream->time_base);
        audFlushPkt->stream_index = outAudStream->index;
        av_interleaved_write_frame(outFmtCtx, audFlushPkt);
    }
    av_packet_free(&audFlushPkt);

    if (cb && duration > 0) cb(duration, duration);

    av_write_trailer(outFmtCtx);

    // Cleanup
    if (!(outFmtCtx->oformat->flags & AVFMT_NOFILE)) avio_closep(&outFmtCtx->pb);
    avformat_free_context(outFmtCtx);
    avcodec_free_context(&audEncCtx);
    avcodec_free_context(&vidEncCtx);
    avfilter_graph_free(&filterGraph);
    av_frame_free(&filtFrame);
    av_frame_free(&audFrame);
    av_packet_free(&pkt);
    av_frame_free(&imgFrame);
    avcodec_free_context(&audDecCtx);
    avformat_close_input(&audFmtCtx);

    return true;
}
