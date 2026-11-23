#ifndef SRC_UTILS_FORMATTER_H_
#define SRC_UTILS_FORMATTER_H_

#include <cstddef>
#include <string>
#include <vector>

template <typename T>
static inline auto to_string(const std::vector<T> &vec) -> std::string {
  std::string str;
  for (const auto &val : vec) {
    str += std::to_string(val) + " ";
  }
  return str;
}

template <typename T> static inline auto to_string(T value) -> std::string {
  return std::to_string(value);
}

template <typename T>
inline auto operator%(std::string format, T value) -> std::string {
  auto const ind = format.find('?');
  if (ind != std::string::npos && (ind == 0 || format.at(ind - 1) != '\\')) {
    format.replace(ind, 1U, to_string(value));
  }
  return format;
}

inline auto operator%(std::string format, const std::string &value)
    -> std::string {
  auto const ind = format.find('?');
  if (ind != std::string::npos && (ind == 0 || format.at(ind - 1) != '\\')) {
    format.replace(ind, 1U, value);
  }
  return format;
}

inline auto operator%(std::string format, const char *value) -> std::string {
  auto const ind = format.find('?');
  if (ind != std::string::npos && (ind == 0 || format.at(ind - 1) != '\\')) {
    format.replace(ind, 1U, value);
  }
  return format;
}

inline auto operator""_f(const char *str, std::size_t size) -> std::string {
  return {str, size};
}

#endif  // SRC_UTILS_FORMATTER_H_
