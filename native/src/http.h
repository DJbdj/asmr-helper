#ifndef HTTP_H
#define HTTP_H

#include <string>

// Pexels API 配置
extern const std::string PEXELS_API_KEY;
extern const std::string PEXELS_BASE_URL;

// 搜索与下载
std::string searchImagePexels(const std::string& query);
bool downloadImage(const std::string& url, const std::string& save_path);

// JSON / URL 工具
std::string urlEncode(const std::string& str);
std::string extractJsonString(const std::string& json, const std::string& key);
std::string decodeJsonUnicode(const std::string& str);
std::string extractPexelsPhotoUrl(const std::string& json);

#endif // HTTP_H
