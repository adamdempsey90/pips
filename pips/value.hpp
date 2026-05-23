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
#define INSTANCE_VAL(ptr) (Value(ptr))
#define VECTOR_VAL(ptr) (Value(ptr))

#define IS_BOOL(value) ((value).type == ValueType::BOOL)
#define IS_NIL(value) ((value).type == ValueType::NIL)
#define IS_NUMBER(value) ((value).type == ValueType::NUMBER)
#define IS_STRING(value) ((value).type == ValueType::STRING)
#define IS_INSTANCE(value) ((value).type == ValueType::INSTANCE)
#define IS_VECTOR(value) ((value).type == ValueType::VECTOR)
#define IS_NUMERIC(value)                                                      \
  ((value).type == ValueType::NUMBER || (value).type == ValueType::BOOL)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_STRING(value) ((value).as.str)
#define AS_INSTANCE(value) ((value).as.instance)
#define AS_VECTOR(value) ((value).as.vector)

extern void printObject(Value val);

inline std::int64_t AS_INTEGER(const Value &val) {
  if (IS_BOOL(val)) {
    return AS_BOOL(val) ? 1 : 0;
  }
  return static_cast<std::int64_t>(AS_NUMBER(val));
}

inline std::int64_t IS_INTEGRAL(const Value &val) {
  if (IS_BOOL(val)) {
    return true;
  }
  if (!IS_NUMBER(val)) {
    return false;
  }
  if (static_cast<Real>(static_cast<std::int64_t>(AS_NUMBER(val))) ==
      AS_NUMBER(val)) {
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
  case ValueType::INSTANCE:
    printf("<instance %p>", static_cast<const void *>(AS_INSTANCE(val)));
    break;
  case ValueType::VECTOR: {
    VectorObject *v = AS_VECTOR(val);
    printf("[");
    if (v) {
      for (size_t i = 0; i < v->elements.size(); ++i) {
        if (i)
          printf(", ");
        printValue(v->elements[i]);
      }
    }
    printf("]");
    break;
  }
  }
}

inline bool stringCompare(Value a, Value b) {
  if (a.type != ValueType::STRING || b.type != ValueType::STRING)
    return false;
  return std::strcmp(a.as.str, b.as.str) == 0;
}
inline bool valuesEqual(Value a, Value b) {
  if (a.type != b.type)
    return false;
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
  case ValueType::INSTANCE:
    return AS_INSTANCE(a) == AS_INSTANCE(b);
  case ValueType::VECTOR: {
    VectorObject *va = AS_VECTOR(a);
    VectorObject *vb = AS_VECTOR(b);
    if (va == vb)
      return true;
    if (!va || !vb)
      return false;
    if (va->elements.size() != vb->elements.size())
      return false;
    for (size_t i = 0; i < va->elements.size(); ++i) {
      if (!valuesEqual(va->elements[i], vb->elements[i]))
        return false;
    }
    return true;
  }
  default:
    return false;
  }
}
} // namespace pips
#endif // PIPS_VALUE_HPP_
