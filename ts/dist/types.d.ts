/**
 * ASMR Helper 类型定义
 */
export type ProcessMode = 'video-replace' | 'audio-to-video';
export type ImageSourceType = 'pexels' | 'url' | 'local';
export interface InputOptions {
    mode: ProcessMode;
    input: string;
    output: string;
    imageSource: ImageSourceType;
    imageInput?: string;
    width?: number;
    height?: number;
}
export interface ProcessResult {
    success: boolean;
    outputPath?: string;
    error?: string;
    duration?: number;
}
export interface VideoInfo {
    width: number;
    height: number;
    duration: number;
    hasAudio: boolean;
}
export interface AudioInfo {
    duration: number;
    format: string;
}
export interface BackendInterface {
    processVideo(input: string, image: string, output: string): Promise<ProcessResult>;
    audioToVideo(audio: string, image: string, output: string, width?: number, height?: number): Promise<ProcessResult>;
    searchImagePexels(query: string): Promise<string>;
    downloadImage(url: string, savePath: string): Promise<boolean>;
    getVideoInfo(path: string): Promise<VideoInfo | null>;
    getAudioInfo(path: string): Promise<AudioInfo | null>;
}
