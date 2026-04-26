#!/usr/bin/env node
/**
 * ASMR Helper CLI
 * TypeScript 命令行接口
 */

import { program } from 'commander';
import * as fs from 'fs';
import * as path from 'path';
import * as os from 'os';
import { backend } from './backend';
import { ProcessMode, ImageSourceType, InputOptions } from './types';

// 临时文件目录
const TEMP_DIR = os.tmpdir();

// 创建临时图片路径
function getTempImagePath(): string {
  return path.join(TEMP_DIR, `asmr-img-${Date.now()}.jpg`);
}

// 获取图片
async function getImage(source: ImageSourceType, input: string): Promise<string | null> {
  if (source === 'local') {
    if (!fs.existsSync(input)) {
      console.error(`错误：图片文件不存在 - ${input}`);
      return null;
    }
    return input;
  }

  if (source === 'url') {
    const tempPath = getTempImagePath();
    console.log('正在下载图片...');
    const success = await backend.downloadImage(input, tempPath);
    if (!success) {
      console.error('错误：图片下载失败');
      return null;
    }
    console.log(`图片已下载: ${tempPath}`);
    return tempPath;
  }

  if (source === 'pexels') {
    console.log(`正在从 Pexels 搜索: ${input}`);
    const url = await backend.searchImagePexels(input);
    if (!url) {
      console.error('错误：未找到图片，请尝试其他关键词');
      return null;
    }
    console.log(`找到图片: ${url}`);

    const tempPath = getTempImagePath();
    console.log('正在下载图片...');
    const success = await backend.downloadImage(url, tempPath);
    if (!success) {
      console.error('错误：图片下载失败');
      return null;
    }
    console.log(`图片已下载: ${tempPath}`);
    return tempPath;
  }

  return null;
}

// 清理临时文件
function cleanupTemp(path: string): void {
  if (path.startsWith(TEMP_DIR) && fs.existsSync(path)) {
    fs.unlinkSync(path);
  }
}

// CLI 主程序
program
  .name('asmr-helper')
  .description('ASMR Helper - 视频源替换工具 CLI')
  .version('1.0.0');

// 视频替换画面命令
program
  .command('video')
  .description('视频替换画面（保留音频）')
  .requiredOption('-i, --input <file>', '输入视频文件')
  .requiredOption('-o, --output <file>', '输出视频文件')
  .option('-q, --query <keyword>', 'Pexels 搜索关键词')
  .option('-u, --url <url>', '图片 URL')
  .option('-f, --file <path>', '本地图片文件')
  .action(async (options) => {
    // 检查输入文件
    if (!fs.existsSync(options.input)) {
      console.error(`错误：视频文件不存在 - ${options.input}`);
      process.exit(1);
    }

    // 确定图片来源
    let sourceType: ImageSourceType;
    let sourceInput: string;

    if (options.query) {
      sourceType = 'pexels';
      sourceInput = options.query;
    } else if (options.url) {
      sourceType = 'url';
      sourceInput = options.url;
    } else if (options.file) {
      sourceType = 'local';
      sourceInput = options.file;
    } else {
      console.error('错误：请指定图片来源（--query, --url 或 --file）');
      process.exit(1);
    }

    // 获取图片
    const imagePath = await getImage(sourceType, sourceInput);
    if (!imagePath) process.exit(1);

    // 处理视频
    console.log('正在处理视频...');
    const result = await backend.processVideo(options.input, imagePath, options.output);

    if (result.success) {
      console.log(`处理完成！输出文件: ${result.outputPath}`);
    } else {
      console.error(`处理失败: ${result.error}`);
    }

    cleanupTemp(imagePath);
    process.exit(result.success ? 0 : 1);
  });

// 音频生成视频命令
program
  .command('audio')
  .description('音频生成视频')
  .requiredOption('-i, --input <file>', '输入音频文件')
  .requiredOption('-o, --output <file>', '输出视频文件')
  .option('-q, --query <keyword>', 'Pexels 搜索关键词')
  .option('-u, --url <url>', '图片 URL')
  .option('-f, --file <path>', '本地图片文件')
  .option('-w, --width <number>', '视频宽度', parseInt, 1920)
  .option('-h, --height <number>', '视频高度', parseInt, 1080)
  .action(async (options) => {
    // 检查输入文件
    if (!fs.existsSync(options.input)) {
      console.error(`错误：音频文件不存在 - ${options.input}`);
      process.exit(1);
    }

    // 确定图片来源
    let sourceType: ImageSourceType;
    let sourceInput: string;

    if (options.query) {
      sourceType = 'pexels';
      sourceInput = options.query;
    } else if (options.url) {
      sourceType = 'url';
      sourceInput = options.url;
    } else if (options.file) {
      sourceType = 'local';
      sourceInput = options.file;
    } else {
      console.error('错误：请指定图片来源（--query, --url 或 --file）');
      process.exit(1);
    }

    // 获取图片
    const imagePath = await getImage(sourceType, sourceInput);
    if (!imagePath) process.exit(1);

    // 生成视频
    console.log(`正在生成视频 (${options.width}x${options.height})...`);
    const result = await backend.audioToVideo(
      options.input,
      imagePath,
      options.output,
      options.width,
      options.height
    );

    if (result.success) {
      console.log(`视频生成完成！输出文件: ${result.outputPath}`);
      console.log(`视频时长: ${result.duration?.toFixed(1)} 秒`);
    } else {
      console.error(`生成失败: ${result.error}`);
    }

    cleanupTemp(imagePath);
    process.exit(result.success ? 0 : 1);
  });

// 搜索图片命令
program
  .command('search')
  .description('搜索 Pexels 图片')
  .requiredOption('-q, --query <keyword>', '搜索关键词')
  .option('-o, --output <path>', '保存路径')
  .action(async (options) => {
    console.log(`正在搜索: ${options.query}`);
    const url = await backend.searchImagePexels(options.query);

    if (!url) {
      console.error('未找到图片');
      process.exit(1);
    }

    console.log(`找到图片: ${url}`);

    if (options.output) {
      console.log('正在下载...');
      const success = await backend.downloadImage(url, options.output);
      if (success) {
        console.log(`图片已保存: ${options.output}`);
      } else {
        console.error('下载失败');
        process.exit(1);
      }
    }

    process.exit(0);
  });

// 信息命令
program
  .command('info')
  .description('获取媒体文件信息')
  .requiredOption('-i, --input <file>', '输入文件')
  .action(async (options) => {
    if (!fs.existsSync(options.input)) {
      console.error(`错误：文件不存在 - ${options.input}`);
      process.exit(1);
    }

    // 尝试获取视频信息
    const videoInfo = await backend.getVideoInfo(options.input);
    if (videoInfo) {
      console.log('视频信息:');
      console.log(`  尺寸: ${videoInfo.width}x${videoInfo.height}`);
      console.log(`  时长: ${videoInfo.duration.toFixed(1)} 秒`);
      console.log(`  音频: ${videoInfo.hasAudio ? '有' : '无'}`);
      process.exit(0);
    }

    // 尝试获取音频信息
    const audioInfo = await backend.getAudioInfo(options.input);
    if (audioInfo) {
      console.log('音频信息:');
      console.log(`  时长: ${audioInfo.duration.toFixed(1)} 秒`);
      console.log(`  格式: ${audioInfo.format}`);
      process.exit(0);
    }

    console.error('无法获取文件信息');
    process.exit(1);
  });

// 解析命令行参数
program.parse();