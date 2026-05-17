#ifndef FRONTEND_PARSER_UTILS_H
#define FRONTEND_PARSER_UTILS_H

#include <cstdlib>
#include <string>

namespace frontend {

// Utility functions for the parser can be defined here

/**
 * @brief Provide functions to help converting string to number
 * especially(maybe only) for literals
 */
static inline auto StringToInt(const std::string &str) -> int { return std::strtol(str.c_str(), nullptr, 0); }

static inline auto StringToFloat(const std::string &str) -> float { return std::strtod(str.c_str(), nullptr); }

}  // namespace frontend

#endif