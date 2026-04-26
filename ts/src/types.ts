/**
 * ASMR Helper 类型定义
 */

// 处理模式
export type ProcessMode = 'video-replace' | 'audio-to-video';

// 图片来源类型
export type ImageSourceType = 'pexels' | 'url' | 'local';

// 输入选项
export interface InputOptions {
  mode: ProcessMode;
  input: string;
  output: string;
  imageSource: ImageSourceType;
  imageInput?: string;  // 关键词、URL 或本地路径
  width?: number;
  height?: number;
}

// 处理结果
export interface ProcessResult {
  success: boolean;
  outputPath?: string;
  error?: string;
  duration?: number;
}

// 视频信息
export interface VideoInfo {
  width: number;
  height: number;
  duration: number;
  hasAudio: boolean;
}

// 音频信息
export interface AudioInfo {
  duration: number;
  format: string;
}

// 后端接口
export interface BackendInterface {
  processVideo(input: string, image: string, output: string): Promise<ProcessResult>;
  audioToVideo(audio: string, image: string, output: string, width?: number, height?: number): Promise<ProcessResult>;
  searchImagePexels(query: string): Promise<string>;
  downloadImage(url: string, savePath: string): Promise<boolean>;
  getVideoInfo(path: string): Promise<VideoInfo | null>;
  getAudioInfo(path: string): Promise<AudioInfo | null>;
}