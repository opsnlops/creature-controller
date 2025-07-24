
#pragma once

#include <fmt/format.h>
#include <stdexcept>

namespace creatures {

/**
 * A runtime error that gets throw if it all goes wrong inside of the logger
 */
class LoggingException : public std::runtime_error {
  public:
    explicit LoggingException(const std::string &msg)
        : std::runtime_error(msg) {}
};

} // namespace creatures