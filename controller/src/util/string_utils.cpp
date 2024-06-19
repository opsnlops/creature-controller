
#include <charconv>
#include <cctype>
#include <string>
#include <vector>
#include <sstream>
#include <cmath> // for std::isspace

#include "string_utils.h"

/**
 * Convert a string into a u32 safely
 *
 * @param str the string to parse
 * @return a u32 of the string, if possible
 */
u32 stringToU32(const std::string& str) {

    // Skip leading whitespace
    const char* start = str.data();
    const char* end = str.data() + str.size();
    while (start < end && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    // Handle hexadecimal input
    int base = 10;
    if ((end - start) >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
        base = 16;
        start += 2; // Skip over the "0x" or "0X" prefix
    }

    u32 value = 0;
    auto result = std::from_chars(start, end, value, base);

    if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
        return 0;
    }

    return value;
}

/**
 * Convert a string into a u64 safely
 *
 * @param str the string to parse
 * @return a u64 of the string, if possible
 */
u64 stringToU64(const std::string& str) {

    // Skip leading whitespace
    const char* start = str.data();
    const char* end = str.data() + str.size();
    while (start < end && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    // Handle hexadecimal input
    int base = 10;
    if ((end - start) >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
        base = 16;
        start += 2; // Skip over the "0x" or "0X" prefix
    }

    u64 value = 0;
    auto result = std::from_chars(start, end, value, base);

    if (result.ec == std::errc::invalid_argument || result.ec == std::errc::result_out_of_range) {
        return 0;
    }

    return value;
}


/**
 * Convert a string into a double safely
 *
 * @param str the string to parse
 * @return a double of the string, if possible, otherwise NaN
 */
double stringToDouble(const std::string& str) {

    // Skip leading whitespace
    const char* start = str.data();
    const char* end = str.data() + str.size();
    while (start < end && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    // Handle potential empty string after trimming whitespace
    if (start == end) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    // Reset errno before calling strtod
    errno = 0;

    // Convert string to double
    char* endPtr;
    double value = std::strtod(start, &endPtr);

    // Check for conversion errors
    if (errno == ERANGE || start == endPtr) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return value;
}



/**
 * Split a string into pieces in a safe way
 *
 * @param str the string to split
 * @return a vector containing the pieces
 */
std::vector<std::string> splitString(const std::string& str) {
    std::istringstream iss(str);
    std::vector<std::string> tokens;

    for (const std::movable auto& part : std::ranges::istream_view<std::string>(iss)) {
        tokens.push_back(part);
    }

    return tokens;
}
