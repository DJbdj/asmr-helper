/**
 * C++ 后端调用接口
 * 通过调用 asmr-helper 可执行文件实现功能
 */
import { ProcessResult, VideoInfo, AudioInfo, BackendInterface } from './types';
export declare class Backend implements BackendInterface {
    private backendPath;
    private ffmpegPath;
    constructor();
    private findFFmpeg;
    /**
     * 视频替换画面
     */
    processVideo(input: string, image: string, output: string): Promise<ProcessResult>;
    /**
     * 音频生成视频
     */
    audioToVideo(audio: string, image: string, output: string, width?: number, height?: number): Promise<ProcessResult>;
    /**
     * Pexels 图片搜索（通过 curl）
     */
    searchImagePexels(query: string): Promise<string>;
    /**
     * 下载图片
     */
    downloadImage(url: string, savePath: string): Promise<boolean>;
    /**
     * 获取视频信息（通过 ffprobe）
     */
    getVideoInfo(videoPath: string): Promise<VideoInfo | null>;
    /**
     * 获取音频信息
     */
    getAudioInfo(audioPath: string): Promise<AudioInfo | null>;
}
export declare const backend: Backend;
