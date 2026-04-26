"use strict";
/**
 * C++ 后端调用接口
 * 通过调用 asmr-helper 可执行文件实现功能
 */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
exports.backend = exports.Backend = void 0;
const child_process_1 = require("child_process");
const path = __importStar(require("path"));
const fs = __importStar(require("fs"));
// 后端可执行文件路径
function getBackendPath() {
    // 开发环境：native/asmr-helper
    const devPath = path.join(__dirname, '../../..', 'native', 'asmr-helper');
    if (fs.existsSync(devPath))
        return devPath;
    // 生产环境：同目录下的 asmr-helper
    const prodPath = path.join(__dirname, '..', 'asmr-helper');
    if (fs.existsSync(prodPath))
        return prodPath;
    // 系统 PATH
    return 'asmr-helper';
}
// 执行命令并获取结果
async function execCommand(command) {
    return new Promise((resolve, reject) => {
        (0, child_process_1.exec)(command, { maxBuffer: 1024 * 1024 * 10 }, (error, stdout, stderr) => {
            if (error)
                reject(new Error(stderr || error.message));
            else
                resolve(stdout);
        });
    });
}
// 后端实现
class Backend {
    constructor() {
        this.backendPath = getBackendPath();
        this.ffmpegPath = this.findFFmpeg();
    }
    findFFmpeg() {
        const ffmpegDir = path.join(path.dirname(this.backendPath), 'ffmpeg', 'ffmpeg');
        if (fs.existsSync(ffmpegDir))
            return ffmpegDir;
        // 系统 ffmpeg
        return 'ffmpeg';
    }
    /**
     * 视频替换画面
     */
    async processVideo(input, image, output) {
        return new Promise((resolve) => {
            const args = [
                '-i', input,
                '-img', image,
                '-o', output
            ];
            const proc = (0, child_process_1.spawn)(this.ffmpegPath, [
                '-y',
                '-i', input,
                '-i', image,
                '-filter_complex', `[1:v]scale=1920:1080:force_original_aspect_ratio=decrease,pad=1920:1080:(ow-iw)/2:(oh-ih)/2[img];[0:v][img]overlay=0:0[outv]`,
                '-map', '[outv]',
                '-map', '0:a?',
                '-c:v', 'libx264',
                '-preset', 'medium',
                '-crf', '23',
                '-c:a', 'copy',
                output
            ]);
            let stderr = '';
            proc.stderr.on('data', (data) => stderr += data.toString());
            proc.on('close', (code) => {
                if (code === 0) {
                    resolve({ success: true, outputPath: output });
                }
                else {
                    resolve({ success: false, error: stderr });
                }
            });
            proc.on('error', (err) => {
                resolve({ success: false, error: err.message });
            });
        });
    }
    /**
     * 音频生成视频
     */
    async audioToVideo(audio, image, output, width = 1920, height = 1080) {
        return new Promise((resolve) => {
            // 先获取音频时长
            this.getAudioInfo(audio).then((info) => {
                if (!info) {
                    resolve({ success: false, error: '无法获取音频信息' });
                    return;
                }
                const proc = (0, child_process_1.spawn)(this.ffmpegPath, [
                    '-y',
                    '-loop', '1',
                    '-i', image,
                    '-i', audio,
                    '-filter_complex', `[0:v]scale=${width}:${height}:force_original_aspect_ratio=decrease,pad=${width}:${height}:(ow-iw)/2:(oh-ih)/2:black[outv]`,
                    '-map', '[outv]',
                    '-map', '1:a',
                    '-c:v', 'libx264',
                    '-preset', 'medium',
                    '-crf', '23',
                    '-c:a', 'aac',
                    '-b:a', '192k',
                    '-t', String(info.duration),
                    '-pix_fmt', 'yuv420p',
                    output
                ]);
                let stderr = '';
                proc.stderr.on('data', (data) => stderr += data.toString());
                proc.on('close', (code) => {
                    if (code === 0) {
                        resolve({ success: true, outputPath: output, duration: info.duration });
                    }
                    else {
                        resolve({ success: false, error: stderr });
                    }
                });
                proc.on('error', (err) => {
                    resolve({ success: false, error: err.message });
                });
            });
        });
    }
    /**
     * Pexels 图片搜索（通过 curl）
     */
    async searchImagePexels(query) {
        const apiKey = 'sEgMIfajDuYkADrF3sfyAuT0vptn1tl1hdswMQi7fJYEHHxbsLexKNPp';
        const url = `https://api.pexels.com/v1/search?query=${encodeURIComponent(query)}&per_page=15&orientation=landscape`;
        const result = await execCommand(`curl -s -H "Authorization: ${apiKey}" "${url}"`);
        // 解析 JSON 提取第一个图片 URL
        try {
            const json = JSON.parse(result);
            if (json.photos && json.photos.length > 0) {
                const photo = json.photos[0];
                return photo.src?.large || photo.src?.original || photo.src?.medium || '';
            }
        }
        catch {
            // JSON 解析失败，尝试简单提取
            const match = result.match(/"large":"([^"]+)"/);
            if (match)
                return match[1].replace(/\\\//g, '/');
        }
        return '';
    }
    /**
     * 下载图片
     */
    async downloadImage(url, savePath) {
        try {
            await execCommand(`curl -s -o "${savePath}" "${url}"`);
            return fs.existsSync(savePath);
        }
        catch {
            return false;
        }
    }
    /**
     * 获取视频信息（通过 ffprobe）
     */
    async getVideoInfo(videoPath) {
        try {
            const result = await execCommand(`ffprobe -v quiet -print_format json -show_streams -show_format "${videoPath}"`);
            const json = JSON.parse(result);
            const videoStream = json.streams?.find((s) => s.codec_type === 'video');
            if (!videoStream)
                return null;
            return {
                width: videoStream.width,
                height: videoStream.height,
                duration: parseFloat(json.format?.duration || videoStream.duration || '0'),
                hasAudio: json.streams?.some((s) => s.codec_type === 'audio')
            };
        }
        catch {
            return null;
        }
    }
    /**
     * 获取音频信息
     */
    async getAudioInfo(audioPath) {
        try {
            const result = await execCommand(`ffprobe -v quiet -print_format json -show_format "${audioPath}"`);
            const json = JSON.parse(result);
            return {
                duration: parseFloat(json.format?.duration || '0'),
                format: json.format?.format_name || 'unknown'
            };
        }
        catch {
            return null;
        }
    }
}
exports.Backend = Backend;
// 默认导出
exports.backend = new Backend();
//# sourceMappingURL=backend.js.map