#include "http_utils.h"

#include <curl/curl.h>

namespace creatures::util {

// Callback function for libcurl to write response data
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool makeHttpPostRequest(const std::string& url,
                        const std::string& body,
                        std::string& responseBody,
                        long& httpCode,
                        std::string& errorMsg,
                        long connectTimeout,
                        long timeout) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        errorMsg = "Failed to initialize curl";
        return false;
    }

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.length());

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set timeouts
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeout);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

    // Set callback for response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    // Perform the request
    CURLcode res = curl_easy_perform(curl);
    bool success = (res == CURLE_OK);

    // Get HTTP response code or error message
    if (success) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    } else {
        errorMsg = curl_easy_strerror(res);
    }

    // Cleanup
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return success;
}

} // namespace creatures::util
