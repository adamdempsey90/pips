#ifndef PIPS_VALUE_HPP_
#define PIPS_VALUE_HPP_

//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================

#include "value_types.hpp"

namespace pips {

#define BOOL_VAL(value) (Value(value))
#define NIL_VAL (Value())
#define NUMBER_VAL(value) (Value(value))
#define STRING_VAL(value) (Value(value))

#define IS_BOOL(value) ((value).type == ValueType::BOOL)
#define IS_NIL(value) ((value).type == ValueType::NIL)
#define IS_NUMBER(value) ((value).type == ValueType::NUMBER)
#define IS_STRING(value) ((value).type == ValueType::STRING)
#define IS_NUMERIC(value) ((value).type == ValueType::NUMBER || (value).type == ValueType::BOOL)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_STRING(value) ((value).as.str)

extern void printObject(Value val);


inline int64_t AS_INTEGER(const Value &val) {
  if (IS_BOOL(val)) {
    return AS_BOOL(val) ? 1 : 0;
  }
  return static_cast<int64_t>(AS_NUMBER(val));
}

inline int64_t IS_INTEGRAL(const Value &val) {
  if (IS_BOOL(val)) {
    return true;
  }
  if (!IS_NUMBER(val)) {
    return false;
  }
  if (static_cast<Real>(static_cast<int64_t>(AS_NUMBER(val))) == AS_NUMBER(val)) {
    return true;
  }
  return false;
}

inline void printValue(const Value &val) {
  switch (val.type) {
  case ValueType::BOOL:
    printf(AS_BOOL(val) ? "true" : "false");
    break;
  case ValueType::NIL:
    printf("nil");
    break;
  case ValueType::NUMBER:
    printf("%.16lg", static_cast<double>(AS_NUMBER(val)));
    break;
  case ValueType::STRING:
    printf("%s", val.as.str);
    break;
  }
}

inline bool stringCompare(Value a, Value b) {
  if (a.type != ValueType::STRING || b.type != ValueType::STRING) return false;
  return std::strcmp(a.as.str, b.as.str) == 0;
}
inline bool valuesEqual(Value a, Value b) {
  if (a.type != b.type) return false;
  switch (a.type) {
  case ValueType::BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case ValueType::NIL:
    return true;
  case ValueType::NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case ValueType::STRING: {
    return stringCompare(a, b);
  }
  default:
    return false;
  }
}
} // namespace pips
#endif // PIPS_VALUE_HPP_
