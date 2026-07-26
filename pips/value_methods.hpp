#ifndef PIPS_VALUE_METHODS_HPP_
#define PIPS_VALUE_METHODS_HPP_

#include <string>

namespace pips {

inline bool is_vector_length_method(const std::string &name) {
  return name == "len" || name == "size";
}

inline bool is_vector_mutating_method(const std::string &name) {
  return name == "push" || name == "pop";
}

inline bool is_string_method(const std::string &name) {
  return is_vector_length_method(name) || name == "strip" ||
         name == "lstrip" || name == "rstrip" || name == "lower" ||
         name == "upper" || name == "starts_with" ||
         name == "ends_with" || name == "contains" || name == "replace" ||
         name == "split";
}

} // namespace pips

#endif // PIPS_VALUE_METHODS_HPP_
