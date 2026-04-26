#include "http.h"
#include "ui.h"
#include <iostream>
#include <cstdio>
#include <vector>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winhttp.h>
    #pragma comment(lib, "winhttp.lib")
#else
    #include <curl/curl.h>
#endif

const std::string PEXELS_API_KEY = "sEgMIfajDuYkADrF3sfyAuT0vptn1tl1hdswMQi7fJYEHHxbsLexKNPp";
const std::string PEXELS_BASE_URL = "https://api.pexels.com/v1/";

// ============== URL 与 JSON 工具 ==============

std::string urlEncode(const std::string& str) {
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    for (char c : str) {
        if (c == ' ') encoded += "%20";
        else if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') encoded += c;
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

// ============== HTTP 请求（跨平台） ==============

// Parse URL into host, path, and whether it's HTTPS
[[maybe_unused]] static bool parseUrl(const std::string& url, std::string& outHost, std::string& outPath, bool& outHttps) {
    size_t protoEnd = url.find("://");
    if (protoEnd == std::string::npos) return false;

    std::string proto = url.substr(0, protoEnd);
    outHttps = (proto == "https");

    size_t hostStart = protoEnd + 3;
    size_t pathStart = url.find('/', hostStart);
    if (pathStart == std::string::npos) {
        outHost = url.substr(hostStart);
        outPath = "/";
    } else {
        outHost = url.substr(hostStart, pathStart - hostStart);
        outPath = url.substr(pathStart);
    }
    return true;
}

#ifdef _WIN32
// Windows: WinHTTP implementation
static bool httpGet(const std::string& url, const std::vector<std::string>& headers,
                    std::string& outBody, long& outStatus, int timeoutMs) {
    std::string host, path;
    bool isHttps;
    if (!parseUrl(url, host, path, isHttps)) return false;

    outStatus = 0;
    outBody.clear();

    // Convert host/path to wide strings
    int hostLen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    std::wstring wHost(hostLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, &wHost[0], hostLen);
    wHost.pop_back(); // remove null terminator

    int pathLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wPath(pathLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], pathLen);
    wPath.pop_back();

    HINTERNET hSession = WinHttpOpen(L"ASMR Helper/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    // Set timeouts
    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    // Ignore SSL certificate errors
    if (isHttps) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }

    // Add custom headers - combine into single CRLF-delimited wide string
    if (!headers.empty()) {
        std::wstring combinedHeaders;
        for (const auto& h : headers) {
            int len = MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, nullptr, 0);
            std::wstring wh(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, &wh[0], len);
            wh.pop_back(); // remove null terminator
            combinedHeaders += wh;
            combinedHeaders += L"\r\n";
        }
        WinHttpAddRequestHeaders(hRequest, combinedHeaders.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Get status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatus = (long)statusCode;

    // Read response body
    char buffer[4096];
    DWORD bytesRead = 0;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        outBody.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

// WinHTTP file download
static bool httpDownloadFile(const std::string& url, const std::string& savePath,
                              const std::vector<std::string>& headers, long& outStatus, int timeoutMs) {
    std::string host, path;
    bool isHttps;
    if (!parseUrl(url, host, path, isHttps)) return false;

    outStatus = 0;

    int hostLen = MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, nullptr, 0);
    std::wstring wHost(hostLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, host.c_str(), -1, &wHost[0], hostLen);
    wHost.pop_back();

    int pathLen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring wPath(pathLen, 0);
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wPath[0], pathLen);
    wPath.pop_back();

    HINTERNET hSession = WinHttpOpen(L"ASMR Helper/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(),
        isHttps ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return false; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return false; }

    if (isHttps) {
        DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                      SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                      SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
    }

    // Add custom headers - combine into single CRLF-delimited wide string
    if (!headers.empty()) {
        std::wstring combinedHeaders;
        for (const auto& h : headers) {
            int len = MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, nullptr, 0);
            std::wstring wh(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, h.c_str(), -1, &wh[0], len);
            wh.pop_back();
            combinedHeaders += wh;
            combinedHeaders += L"\r\n";
        }
        WinHttpAddRequestHeaders(hRequest, combinedHeaders.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);
    outStatus = (long)statusCode;

    FILE* file = fopen(savePath.c_str(), "wb");
    if (!file) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return false;
    }

    char buffer[8192];
    DWORD bytesRead = 0;
    bool success = true;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        if (fwrite(buffer, 1, bytesRead, file) != bytesRead) {
            success = false;
            break;
        }
    }
    fclose(file);

    if (!success) {
        remove(savePath.c_str());
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}
#else
// Unix: CURL implementation
static size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t total = size * nmemb;
    userp->append((char*)contents, total);
    return total;
}

static bool httpGet(const std::string& url, const std::vector<std::string>& headers,
                    std::string& outBody, long& outStatus, int timeoutMs) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    outStatus = 0;
    outBody.clear();

    struct curl_slist* list = nullptr;
    for (const auto& h : headers) {
        list = curl_slist_append(list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBody);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeoutMs);

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &outStatus);
    }

    curl_slist_free_all(list);
    curl_easy_cleanup(curl);
    return (res == CURLE_OK);
}

static bool httpDownloadFile(const std::string& url, const std::string& savePath,
                              const std::vector<std::string>& headers, long& outStatus, int timeoutMs) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    outStatus = 0;

    FILE* file = fopen(savePath.c_str(), "wb");
    if (!file) { curl_easy_cleanup(curl); return false; }

    struct curl_slist* list = nullptr;
    for (const auto& h : headers) {
        list = curl_slist_append(list, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeoutMs);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &outStatus);

    fclose(file);
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || outStatus != 200) {
        remove(savePath.c_str());
        return false;
    }

    // Check file is not empty
    FILE* check = fopen(savePath.c_str(), "rb");
    if (check) {
        fseek(check, 0, SEEK_END);
        long size = ftell(check);
        fclose(check);
        if (size == 0) {
            remove(savePath.c_str());
            return false;
        }
    }
    return true;
}
#endif

// ============== Pexels 搜索 ==============

std::string searchImagePexels(const std::string& query) {
    std::string url = PEXELS_BASE_URL + "search?query=" + urlEncode(query) + "&per_page=15&orientation=landscape";

    std::vector<std::string> headers;
    headers.push_back("Authorization: " + PEXELS_API_KEY);

    std::string body;
    long status = 0;
    if (!httpGet(url, headers, body, status, 30000)) return "";
    if (status != 200) return "";

    return extractPexelsPhotoUrl(body);
}

// ============== 图片下载 ==============

bool downloadImage(const std::string& url, const std::string& save_path) {
    std::vector<std::string> headers;
    headers.push_back("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    headers.push_back("Accept: image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8");
    headers.push_back("Accept-Language: zh-CN,zh;q=0.9,en;q=0.8");
    headers.push_back("Referer: https://www.msn.com/");
    headers.push_back("Cache-Control: no-cache");
    headers.push_back("Pragma: no-cache");

    long status = 0;
    return httpDownloadFile(url, save_path, headers, status, 120000);
}
