#ifndef PIPS_UTILS_HPP_
#define PIPS_UTILS_HPP_

#include <limits>
#include <string>

namespace pips {

namespace Utils {

template <typename T>
inline constexpr auto Big() {
  return std::numeric_limits<T>::max();
}

inline char *readFile(std::string path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    std::fprintf(stderr, "Could not open file \"%s\".\n", path.c_str());
    exit(74);
  }
  std::fseek(file, 0L, SEEK_END);
  size_t fileSize = ftell(file);
  std::rewind(file);

  char *buffer = new char[fileSize + 1];
  size_t bytesRead = std::fread(buffer, sizeof(char), fileSize, file);
  buffer[bytesRead] = '\0';

  std::fclose(file);
  return buffer;
}

template <typename E>
constexpr auto to_underlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

inline std::string getKey(const char *chars) {
  std::string key = chars;
  return key;
}
} // namespace Utils
} // namespace pips
#endif // PIPS_UTILS_HPP_
