#include "http_utils.h"

#include <curl/curl.h>

namespace creatures::util {

// Callback function for libcurl to write response data
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    static_cast<std::string *>(userp)->append(static_cast<char *>(contents), size * nmemb);
    return size * nmemb;
}

namespace {

bool makeHttpRequest(const std::string &url, const std::string *body, std::string &responseBody, long &httpCode,
                     std::string &errorMsg, long connectTimeout, long timeout) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        errorMsg = "Failed to initialize curl";
        return false;
    }

    struct curl_slist *headers = nullptr;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

    if (body) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body->c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body->length());
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);

    if (success) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    } else {
        errorMsg = curl_easy_strerror(res);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

} // namespace

bool makeHttpPostRequest(const std::string &url, const std::string &body, std::string &responseBody, long &httpCode,
                         std::string &errorMsg, long connectTimeout, long timeout) {
    return makeHttpRequest(url, &body, responseBody, httpCode, errorMsg, connectTimeout, timeout);
}

bool makeHttpGetRequest(const std::string &url, std::string &responseBody, long &httpCode, std::string &errorMsg,
                        long connectTimeout, long timeout) {
    return makeHttpRequest(url, nullptr, responseBody, httpCode, errorMsg, connectTimeout, timeout);
}

} // namespace creatures::util
