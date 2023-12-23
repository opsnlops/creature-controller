
#pragma once

#include <string>
#include <charconv>
#include <cstdint>
#include <vector>
#include <ranges>
#include <sstream>

#include "controller-config.h"

/**
 * Convert a string into a u32 safely
 *
 * @param str the string to parse
 * @return a u32 of the string, if possible
 */
u32 stringToU32(const std::string& str);

/**
 * Convert a string into a u64 safely
 *
 * @param str the string to parse
 * @return a u64 of the string, if possible
 */
u64 stringToU64(const std::string& str);

/**
 * Split a string into pieces in a safe way
 *
 * @param str the string to split
 * @return a vector containing the pieces
 */
std::vector<std::string> splitString(const std::string& str);
