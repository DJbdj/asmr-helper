#include "http.h"
#include "ui.h"
#include <curl/curl.h>
#include <iostream>
#include <cstdio>

const std::string PEXELS_API_KEY = "sEgMIfajDuYkADrF3sfyAuT0vptn1tl1hdswMQi7fJYEHHxbsLexKNPp";
const std::string PEXELS_BASE_URL = "https://api.pexels.com/v1/";

// ============== CURL 回调 ==============

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total_size = size * nmemb;
    userp->append((char*)contents, total_size);
    return total_size;
}

// ============== URL 与 JSON 工具 ==============

std::string urlEncode(const std::string& str) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (char c : str) {
        if (c == ' ') encoded += "%20";
        else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += c;
        else {
            encoded += '%';
            encoded += hex[(unsigned char)c >> 4];
            encoded += hex[(unsigned char)c & 0x0F];
        }
    }
    return encoded;
}

std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);
    if (key_pos == std::string::npos) return "";

    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) return "";

    size_t start_pos = colon_pos + 1;
    while (start_pos < json.length() && json[start_pos] == ' ') start_pos++;

    if (start_pos >= json.length()) return "";

    if (json[start_pos] == '"') {
        size_t end_pos = json.find('"', start_pos + 1);
        if (end_pos == std::string::npos) return "";
        return json.substr(start_pos + 1, end_pos - start_pos - 1);
    }

    size_t end_pos = start_pos;
    while (end_pos < json.length() && json[end_pos] != ',' && json[end_pos] != '}' && json[end_pos] != '\n') {
        end_pos++;
    }
    return json.substr(start_pos, end_pos - start_pos);
}

std::string decodeJsonUnicode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            if (str[i + 1] == 'u' && i + 5 < str.length()) {
                std::string hex = str.substr(i + 2, 4);
                unsigned int code = 0;
                for (char c : hex) {
                    code <<= 4;
                    if (c >= '0' && c <= '9') code |= (c - '0');
                    else if (c >= 'a' && c <= 'f') code |= (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F') code |= (c - 'A' + 10);
                }
                result += (char)code;
                i += 5;
            } else {
                result += str[i + 1];
                i++;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string extractPexelsPhotoUrl(const std::string& json) {
    size_t photos_pos = json.find("\"photos\"");
    if (photos_pos == std::string::npos) return "";

    size_t array_start = json.find('[', photos_pos);
    if (array_start == std::string::npos) return "";

    size_t photo_start = json.find('{', array_start);
    if (photo_start == std::string::npos) return "";

    int brace_count = 1;
    size_t photo_end = photo_start + 1;
    while (photo_end < json.length() && brace_count > 0) {
        if (json[photo_end] == '{') brace_count++;
        else if (json[photo_end] == '}') brace_count--;
        photo_end++;
    }

    std::string photo_json = json.substr(photo_start, photo_end - photo_start);

    size_t src_pos = photo_json.find("\"src\"");
    if (src_pos == std::string::npos) return "";

    size_t src_start = photo_json.find('{', src_pos);
    if (src_start == std::string::npos) return "";

    int src_brace = 1;
    size_t src_end = src_start + 1;
    while (src_end < photo_json.length() && src_brace > 0) {
        if (photo_json[src_end] == '{') src_brace++;
        else if (photo_json[src_end] == '}') src_brace--;
        src_end++;
    }

    std::string src_json = photo_json.substr(src_start, src_end - src_start);

    std::vector<std::string> priorities = {"large", "large2x", "original", "medium", "small"};
    for (const auto& key : priorities) {
        std::string url = extractJsonString(src_json, key);
        if (!url.empty() && url.find("http") == 0) {
            return decodeJsonUnicode(url);
        }
    }
    return "";
}

// ============== Pexels 搜索 ==============

std::string searchImagePexels(const std::string& query) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    std::string url = PEXELS_BASE_URL + "search?query=" + urlEncode(query) + "&per_page=15&orientation=landscape";

    std::string response_body;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("Authorization: " + PEXELS_API_KEY).c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    std::string image_url;

    if (res == CURLE_OK) {
        image_url = extractPexelsPhotoUrl(response_body);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return image_url;
}

// ============== 图片下载 ==============

bool downloadImage(const std::string& url, const std::string& save_path) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "  CURL 初始化失败" << std::endl;
        return false;
    }

    FILE* file = fopen(save_path.c_str(), "wb");
    if (!file) {
        std::cerr << "  无法创建文件：" << save_path << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    headers = curl_slist_append(headers, "Accept: image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
    headers = curl_slist_append(headers, "Referer: https://www.msn.com/");
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    headers = curl_slist_append(headers, "Pragma: no-cache");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    fclose(file);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "  CURL 错误：" << curl_easy_strerror(res) << std::endl;
        remove(save_path.c_str());
        return false;
    }

    if (http_code != 200) {
        std::cerr << "  HTTP 状态码：" << http_code << std::endl;
        remove(save_path.c_str());
        return false;
    }

    FILE* check = fopen(save_path.c_str(), "rb");
    if (check) {
        fseek(check, 0, SEEK_END);
        long size = ftell(check);
        fclose(check);
        if (size == 0) {
            std::cerr << "  下载文件为空" << std::endl;
            remove(save_path.c_str());
            return false;
        }
    }

    return true;
}
