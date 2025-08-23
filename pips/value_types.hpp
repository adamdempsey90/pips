#ifndef PIPS_VALUE_TYPES_HPP_
#define PIPS_VALUE_TYPES_HPP_

//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================
#include "types.hpp"
#include <cstring>

namespace pips {

inline void StringToChar(std::string str, char *buff) {
  size_t len = std::min(str.length(), static_cast<size_t>(STRING_MAX - 1));
  std::memcpy(buff, str.c_str(), len);
  buff[len] = '\0';
}

enum class ValueType { BOOL, NIL, STRING, NUMBER };

struct Value {
  ValueType type;
  union {
    bool boolean;
    Real number;
    char str[STRING_MAX];
  } as;

  Value() {
    type = ValueType::NIL;
    as.number = 0;
  }
  template <typename T>
  Value(T v) {
    if constexpr (std::is_same_v<T, std::string>) {
      type = ValueType::STRING;
      StringToChar(v, as.str);
    } else if constexpr (std::is_same_v<T, bool>) {
      type = ValueType::BOOL;
      as.boolean = v;
    } else if constexpr (std::is_arithmetic_v<T>) {
      type = ValueType::NUMBER;
      as.number = static_cast<Real>(v);
    } else {
      static_assert("Unsupported type for Value");
    }
  }

  Value(const Value &other) {
    type = other.type;
    switch (type) {
    case ValueType::BOOL:
      as.boolean = other.as.boolean;
      break;
    case ValueType::NIL:
      as.number = 0;
      break;
    case ValueType::STRING:
      std::strcpy(as.str, other.as.str);
      break;
    case ValueType::NUMBER:
      as.number = other.as.number;
      break;
    }
  }

  Value &operator=(const Value &other) {
    if (this != &other) {
      type = other.type;
      switch (type) {
      case ValueType::BOOL:
        as.boolean = other.as.boolean;
        break;
      case ValueType::NIL:
        as.number = 0;
        break;
      case ValueType::STRING:
        std::strcpy(as.str, other.as.str);
        break;
      case ValueType::NUMBER:
        as.number = other.as.number;
        break;
      }
    }
    return *this;
  }
};

} // namespace pips
#endif // PIPS_VALUE_TYPES_HPP_
