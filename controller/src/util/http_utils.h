#pragma once

#include <string>

namespace creatures::util {

/**
 * @brief Make an HTTP POST request using libcurl
 *
 * @param url The URL to POST to
 * @param body The request body (JSON string)
 * @param responseBody Output parameter for the response body
 * @param httpCode Output parameter for the HTTP status code
 * @param errorMsg Output parameter for error message if request fails
 * @param connectTimeout Connection timeout in seconds (default: 10)
 * @param timeout Total request timeout in seconds (default: 30)
 * @return true if the request succeeded, false otherwise
 */
bool makeHttpPostRequest(const std::string &url, const std::string &body, std::string &responseBody, long &httpCode,
                         std::string &errorMsg, long connectTimeout = 10L, long timeout = 30L);

/**
 * @brief Make an HTTP GET request using libcurl
 *
 * @param url The URL to GET
 * @param responseBody Output parameter for the response body
 * @param httpCode Output parameter for the HTTP status code
 * @param errorMsg Output parameter for error message if request fails
 * @param connectTimeout Connection timeout in seconds (default: 10)
 * @param timeout Total request timeout in seconds (default: 30)
 * @return true if the request succeeded, false otherwise
 */
bool makeHttpGetRequest(const std::string &url, std::string &responseBody, long &httpCode, std::string &errorMsg,
                        long connectTimeout = 10L, long timeout = 30L);

} // namespace creatures::util
