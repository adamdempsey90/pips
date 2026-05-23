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
#include <string>
#include <type_traits>
#include <vector>

namespace pips {

enum class ValueType { BOOL, NIL, STRING, NUMBER, INSTANCE, VECTOR };

struct Instance;     // forward declaration; defined in object.hpp
struct VectorObject; // forward declaration; defined below
struct StringObject; // forward declaration; defined below

struct Value {
  ValueType type;
  union {
    bool boolean;
    Real number;
    StringObject *string;
    Instance *instance;
    VectorObject *vector;
  } as;

  Value() {
    type = ValueType::NIL;
    as.number = 0;
  }
  template <typename T> Value(T v) {
    if constexpr (std::is_same_v<T, bool>) {
      type = ValueType::BOOL;
      as.boolean = v;
    } else if constexpr (std::is_pointer_v<T> &&
                         std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,
                                        StringObject>) {
      type = ValueType::STRING;
      as.string = v;
    } else if constexpr (std::is_pointer_v<T> &&
                         std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,
                                        Instance>) {
      type = ValueType::INSTANCE;
      as.instance = v;
    } else if constexpr (std::is_pointer_v<T> &&
                         std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>,
                                        VectorObject>) {
      type = ValueType::VECTOR;
      as.vector = v;
    } else if constexpr (std::is_arithmetic_v<T>) {
      type = ValueType::NUMBER;
      as.number = static_cast<Real>(v);
    } else {
      static_assert(!std::is_same_v<T, T>, "Unsupported type for Value");
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
      as.string = other.as.string;
      break;
    case ValueType::NUMBER:
      as.number = other.as.number;
      break;
    case ValueType::INSTANCE:
      as.instance = other.as.instance;
      break;
    case ValueType::VECTOR:
      as.vector = other.as.vector;
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
        as.string = other.as.string;
        break;
      case ValueType::NUMBER:
        as.number = other.as.number;
        break;
      case ValueType::INSTANCE:
        as.instance = other.as.instance;
        break;
      case ValueType::VECTOR:
        as.vector = other.as.vector;
        break;
      }
    }
    return *this;
  }
};

struct StringObject {
  std::string str;
};

struct VectorObject {
  std::vector<Value> elements;
};

} // namespace pips
#endif // PIPS_VALUE_TYPES_HPP_
