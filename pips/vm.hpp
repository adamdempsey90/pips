#ifndef PIPS_VM_HPP_
#define PIPS_VM_HPP_
//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================

// #define DEBUG_TRACE_EXECUTION

#include <cstring>
#include <iostream>
#include <memory>
#include <stdarg.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "chunk.hpp"
#include "compiler.hpp"
#include "function.hpp"
#include "math.hpp"
#include "object.hpp"
#include "readline.hpp"
#include "scanner.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "value.hpp"
#include "value_methods.hpp"

namespace pips {

using VTable = std::unordered_map<std::string, Value>;

enum class InterpretResult { OK, COMPILE_ERROR, RUNTIME_ERROR };
// ObjString *takeString(VM *vm, char *chars, int length);

// NOTE: The VM needs to be runnable on device and host, so limit the
//       use of data structures used (e.g., no std::vector)
//   But does the compiler also need to run on device?????

// TODO: convert this to member function of VM
#define BINARY_OP(valueType, op)                                               \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    Real b = AS_NUMBER(pop());                                                 \
    Real a = AS_NUMBER(pop());                                                 \
    push(valueType(a op b));                                                   \
  } while (false)

#define MOD_OP(valueType)                                                      \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
                                                                               \
      runtimeError("Operands must be numbers.");                               \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    Real b = AS_NUMBER(pop());                                                 \
    Real a = AS_NUMBER(pop());                                                 \
    push(valueType(                                                            \
        static_cast<Real>(static_cast<int>(a) % static_cast<int>(b))));        \
  } while (false)

#define INTDIVIDE_OP(valueType)                                                \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
                                                                               \
      runtimeError("Operands must be numbers.");                               \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    Real b = AS_NUMBER(pop());                                                 \
    Real a = AS_NUMBER(pop());                                                 \
    push(valueType(static_cast<Real>(static_cast<int>(a / b))));               \
  } while (false)

#define STD_BINARY_OP(func, valueType)                                         \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                          \
      runtimeError("Operands must be numbers.");                               \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    Real b = AS_NUMBER(pop());                                                 \
    Real a = AS_NUMBER(pop());                                                 \
    push(valueType(func(a, b)));                                               \
  } while (false)

#define BITWISE_OP(op)                                                         \
  do {                                                                         \
    if (!IS_INTEGRAL(peek(0)) || !IS_INTEGRAL(peek(1))) {                      \
      runtimeError("Operands must be convertable to integers.");               \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    if (IS_BOOL(peek(0)) && IS_BOOL(peek(1))) {                                \
      bool b = AS_BOOL(pop());                                                 \
      bool a = AS_BOOL(pop());                                                 \
      push(BOOL_VAL(a op b));                                                  \
    } else {                                                                   \
      std::int64_t b = AS_INTEGER(pop());                                      \
      std::int64_t a = AS_INTEGER(pop());                                      \
      push(NUMBER_VAL(a op b));                                                \
    }                                                                          \
  } while (false)

#define UNARY_OP(func)                                                         \
  do {                                                                         \
    if (!IS_NUMBER(peek(0)) && !IS_VECTOR(peek(0))) {                          \
      runtimeError("Operand must be a number or a vector.");                   \
      return InterpretResult::RUNTIME_ERROR;                                   \
    }                                                                          \
    if (IS_NUMBER(peek(0))) {                                                  \
      push(NUMBER_VAL(func(AS_NUMBER(pop()))));                                \
    } else {                                                                   \
      VectorObject *vec = AS_VECTOR(pop());                                    \
      VectorObject *result = newVector();                                      \
      for (const auto &e : vec->elements) {                                    \
        if (!IS_NUMBER(e)) {                                                   \
          runtimeError("Vector operand must contain only numbers.");           \
          return InterpretResult::RUNTIME_ERROR;                               \
        }                                                                      \
        result->elements.push_back(NUMBER_VAL(func(AS_NUMBER(e))));            \
      }                                                                        \
      push(VECTOR_VAL(result));                                                \
    }                                                                          \
  } while (false)
struct VM {
  Chunk *chunk = nullptr;
  std::uint8_t *ip = nullptr;
  Value stack[STACK_MAX];
  Value *stackTop;
  Value *frameBase;

  // Table strings;
  VTable globals;

  // Table of functions
  std::unordered_map<std::string, Function> functions;

  // Table of classes
  std::unordered_map<std::string, ClassDef> classes;
  // Owning storage for runtime instance objects.
  std::vector<std::unique_ptr<Instance>> instances;
  // Owning storage for runtime vector objects.
  std::vector<std::unique_ptr<VectorObject>> vectors;
  // Owning storage for runtime string objects.
  std::vector<std::unique_ptr<StringObject>> strings;

  CallFrame frames[FRAMES_MAX];
  int frameCount = 0;

  VM() { resetExecutionState(); }
  VM(const VM &) = delete;
  VM &operator=(const VM &) = delete;
  VM(VM &&other) noexcept { moveFrom(std::move(other)); }
  VM &operator=(VM &&other) noexcept {
    if (this != &other) {
      globals = std::move(other.globals);
      functions = std::move(other.functions);
      classes = std::move(other.classes);
      instances = std::move(other.instances);
      vectors = std::move(other.vectors);
      strings = std::move(other.strings);
      resetExecutionState();
      other.resetExecutionState();
    }
    return *this;
  }
  ~VM() = default;

  void resetExecutionState() {
    chunk = nullptr;
    ip = nullptr;
    stackTop = stack;
    frameBase = stack;
    frameCount = 0;
    for (auto &frame : frames) {
      frame = {};
    }
  }

  void moveFrom(VM &&other) noexcept {
    globals = std::move(other.globals);
    functions = std::move(other.functions);
    classes = std::move(other.classes);
    instances = std::move(other.instances);
    vectors = std::move(other.vectors);
    strings = std::move(other.strings);
    resetExecutionState();
    other.resetExecutionState();
  }




  void runtimeError(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputs("\n", stderr);

    size_t instruction = ip - chunk->code.data() - 1;
    int line = chunk->lines[instruction];
    std::fprintf(stderr, "[line %d] in script\n", line);
    stackTop = stack;
  }

  bool push(Value val) {
    if (stackTop >= stack + STACK_MAX) {
      runtimeError("PIPS Stack overflow.");
      return false;
    }
    *stackTop = val;
    stackTop++;
    return true;
  }
  Value pop() {
    stackTop--;
    return *stackTop;
  }
  Value peek(int dist) { return stackTop[-1 - dist]; }
  bool isFalsey(Value val) {
    return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val)) ||
           (IS_INTEGRAL(val) && AS_INTEGER(val) == 0);
  }
  void concatenate() {
    std::string a_str = AS_STD_STRING(pop());
    std::string b_str = AS_STD_STRING(pop());
    push(STRING_VAL(newString(b_str + a_str)));
  }

  static void hostError(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    std::fputc('\n', stderr);
  }

  StringObject *newString(std::string s) {
    strings.push_back(std::make_unique<StringObject>());
    strings.back()->str = std::move(s);
    return strings.back().get();
  }

  Value makeString(std::string s) {
    return STRING_VAL(newString(std::move(s)));
  }
  Value makeString(const char *s) { return makeString(std::string(s)); }

  VectorObject *newVector() {
    vectors.push_back(std::make_unique<VectorObject>());
    return vectors.back().get();
  }

  // Apply a scalar arithmetic op identified by `op` ('+', '-', '*', '/', '%',
  // '^' for pow). Returns false if the op is unknown.
  static bool applyScalarOp(Real a, Real b, char op, Real &out) {
    switch (op) {
    case '+': out = a + b; return true;
    case '-': out = a - b; return true;
    case '*': out = a * b; return true;
    case '/': out = a / b; return true;
    case '%':
      out = static_cast<Real>(static_cast<long long>(a) %
                              static_cast<long long>(b));
      return true;
    case '^': out = std::pow(a, b); return true;
    }
    return false;
  }

  // Element-wise / broadcasting arithmetic. Pops two values, pushes result.
  InterpretResult binaryArith(char op) {
    Value b = pop();
    Value a = pop();
    bool av = IS_VECTOR(a);
    bool bv = IS_VECTOR(b);
    if (!av && !bv) {
      if (!IS_NUMBER(a) || !IS_NUMBER(b)) {
        runtimeError("Operands must be numbers.");
        return InterpretResult::RUNTIME_ERROR;
      }
      Real r;
      applyScalarOp(AS_NUMBER(a), AS_NUMBER(b), op, r);
      push(NUMBER_VAL(r));
      return InterpretResult::OK;
    }
    VectorObject *out = newVector();
    // Combine two elements (or a scalar with an element) according to op.
    auto combine = [&](const Value &x, const Value &y, Value &result) -> bool {
      if (op == '+' && IS_STRING(x) && IS_STRING(y)) {
        result = STRING_VAL(newString(AS_STD_STRING(x) + AS_STD_STRING(y)));
        return true;
      }
      if (!IS_NUMBER(x) || !IS_NUMBER(y)) {
        if (op == '+')
          runtimeError("Vector '+' requires numeric or string operands.");
        else
          runtimeError("Vector arithmetic requires numeric elements.");
        return false;
      }
      Real r;
      applyScalarOp(AS_NUMBER(x), AS_NUMBER(y), op, r);
      result = NUMBER_VAL(r);
      return true;
    };
    if (av && bv) {
      auto &ae = AS_VECTOR(a)->elements;
      auto &be = AS_VECTOR(b)->elements;
      if (ae.size() != be.size()) {
        runtimeError("Vector size mismatch: %zu vs %zu.", ae.size(), be.size());
        return InterpretResult::RUNTIME_ERROR;
      }
      out->elements.reserve(ae.size());
      for (size_t i = 0; i < ae.size(); ++i) {
        Value r;
        if (!combine(ae[i], be[i], r))
          return InterpretResult::RUNTIME_ERROR;
        out->elements.push_back(r);
      }
    } else if (av) {
      auto &ae = AS_VECTOR(a)->elements;
      out->elements.reserve(ae.size());
      for (const auto &e : ae) {
        Value r;
        if (!combine(e, b, r))
          return InterpretResult::RUNTIME_ERROR;
        out->elements.push_back(r);
      }
    } else {
      auto &be = AS_VECTOR(b)->elements;
      out->elements.reserve(be.size());
      for (const auto &e : be) {
        Value r;
        if (!combine(a, e, r))
          return InterpretResult::RUNTIME_ERROR;
        out->elements.push_back(r);
      }
    }
    push(VECTOR_VAL(out));
    return InterpretResult::OK;
  }

  // Normalize a single index, supporting negative indexing.
  bool normalizeIndex(std::int64_t raw, size_t size, size_t &out) {
    std::int64_t i = raw;
    if (i < 0) i += static_cast<std::int64_t>(size);
    if (i < 0 || static_cast<size_t>(i) >= size) {
      runtimeError("Vector index %lld out of range for size %zu.",
                   static_cast<long long>(raw), size);
      return false;
    }
    out = static_cast<size_t>(i);
    return true;
  }

  // Normalize slice bounds: nil bounds become 0 / size; negatives wrap once.
  // Bounds outside [-size, size] raise a runtime error. Requires lo <= hi.
  bool normalizeSliceBounds(const Value &loV, const Value &hiV, size_t size,
                            size_t &loOut, size_t &hiOut) {
    auto norm = [&](const Value &v, std::int64_t deflt, const char *name,
                    std::int64_t &out) -> bool {
      if (IS_NIL(v)) { out = deflt; return true; }
      std::int64_t x = AS_INTEGER(v);
      std::int64_t orig = x;
      if (x < 0) x += static_cast<std::int64_t>(size);
      if (x < 0 || x > static_cast<std::int64_t>(size)) {
        runtimeError("Slice %s bound %lld out of range for size %zu.", name,
                     static_cast<long long>(orig), size);
        return false;
      }
      out = x;
      return true;
    };
    std::int64_t l, h;
    if (!norm(loV, 0, "lower", l)) return false;
    if (!norm(hiV, static_cast<std::int64_t>(size), "upper", h)) return false;
    if (h < l) {
      runtimeError("Slice upper bound %lld is less than lower bound %lld.",
                   static_cast<long long>(h), static_cast<long long>(l));
      return false;
    }
    loOut = static_cast<size_t>(l);
    hiOut = static_cast<size_t>(h);
    return true;
  }

  bool methodArity(const std::string &name, std::uint8_t argc,
                   std::uint8_t minArgs, std::uint8_t maxArgs) {
    if (argc >= minArgs && argc <= maxArgs) return true;
    if (minArgs == maxArgs) {
      runtimeError("Method '%s' expects %u argument(s), got %u.", name.c_str(),
                   static_cast<unsigned>(minArgs),
                   static_cast<unsigned>(argc));
    } else {
      runtimeError("Method '%s' expects %u to %u arguments, got %u.",
                   name.c_str(), static_cast<unsigned>(minArgs),
                   static_cast<unsigned>(maxArgs),
                   static_cast<unsigned>(argc));
    }
    return false;
  }

  bool methodStringArg(const std::string &method, Value value,
                       std::string &out) {
    if (!IS_STRING(value)) {
      runtimeError("Method '%s' expects string arguments.", method.c_str());
      return false;
    }
    out = AS_STD_STRING(value);
    return true;
  }

  static bool valueString(Value value, std::string &out) {
    switch (value.type) {
    case ValueType::BOOL:
      out = AS_BOOL(value) ? "true" : "false";
      return true;
    case ValueType::NUMBER: {
      char buffer[64];
      std::snprintf(buffer, sizeof(buffer), "%.16lg",
                    static_cast<double>(AS_NUMBER(value)));
      out = buffer;
      return true;
    }
    case ValueType::STRING:
      out = AS_STD_STRING(value);
      return true;
    case ValueType::NIL:
      out = "nil";
      return true;
    default:
      return false;
    }
  }

  InterpretResult callVectorMethod(const std::string &name,
                                   std::uint8_t argc) {
    Value *receiverSlot = stackTop - argc - 1;
    VectorObject *vector = AS_VECTOR(*receiverSlot);
    if (is_vector_length_method(name)) {
      if (!methodArity(name, argc, 0, 0))
        return InterpretResult::RUNTIME_ERROR;
      *receiverSlot = NUMBER_VAL(static_cast<Real>(vector->elements.size()));
      stackTop = receiverSlot + 1;
      return InterpretResult::OK;
    }
    if (name == "push") {
      if (!methodArity(name, argc, 1, 1))
        return InterpretResult::RUNTIME_ERROR;
      Value value = receiverSlot[1];
      vector->elements.push_back(value);
      *receiverSlot = NIL_VAL;
      stackTop = receiverSlot + 1;
      return InterpretResult::OK;
    }
    if (name == "pop") {
      if (!methodArity(name, argc, 0, 0))
        return InterpretResult::RUNTIME_ERROR;
      if (vector->elements.empty()) {
        runtimeError("Cannot pop from an empty vector.");
        return InterpretResult::RUNTIME_ERROR;
      }
      Value value = vector->elements.back();
      vector->elements.pop_back();
      *receiverSlot = value;
      stackTop = receiverSlot + 1;
      return InterpretResult::OK;
    }
    if (name == "join") {
      if (!methodArity(name, argc, 1, 1))
        return InterpretResult::RUNTIME_ERROR;
      std::string separator;
      if (!methodStringArg(name, receiverSlot[1], separator))
        return InterpretResult::RUNTIME_ERROR;
      std::string joined;
      for (std::size_t i = 0; i < vector->elements.size(); ++i) {
        std::string element;
        if (!valueString(vector->elements[i], element)) {
          runtimeError("Method 'join' cannot convert vector element %zu to a "
                       "string.",
                       i);
          return InterpretResult::RUNTIME_ERROR;
        }
        if (i != 0) joined += separator;
        joined += element;
      }
      *receiverSlot = STRING_VAL(newString(std::move(joined)));
      stackTop = receiverSlot + 1;
      return InterpretResult::OK;
    }
    runtimeError("Undefined vector method '%s'.", name.c_str());
    return InterpretResult::RUNTIME_ERROR;
  }

  static bool asciiWhitespace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' ||
           c == '\v';
  }

  InterpretResult callStringMethod(const std::string &name,
                                   std::uint8_t argc) {
    Value *receiverSlot = stackTop - argc - 1;
    const std::string source = AS_STD_STRING(*receiverSlot);
    Value result = NIL_VAL;

    if (is_vector_length_method(name)) {
      if (!methodArity(name, argc, 0, 0))
        return InterpretResult::RUNTIME_ERROR;
      result = NUMBER_VAL(static_cast<Real>(source.size()));
    } else if (name == "lower" || name == "upper") {
      if (!methodArity(name, argc, 0, 0))
        return InterpretResult::RUNTIME_ERROR;
      std::string transformed = source;
      for (char &c : transformed) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (name == "lower" && uc >= 'A' && uc <= 'Z')
          c = static_cast<char>(uc + ('a' - 'A'));
        else if (name == "upper" && uc >= 'a' && uc <= 'z')
          c = static_cast<char>(uc - ('a' - 'A'));
      }
      result = STRING_VAL(newString(std::move(transformed)));
    } else if (name == "strip" || name == "lstrip" || name == "rstrip") {
      if (!methodArity(name, argc, 0, 1))
        return InterpretResult::RUNTIME_ERROR;
      std::string chars;
      if (argc == 1 && !methodStringArg(name, receiverSlot[1], chars))
        return InterpretResult::RUNTIME_ERROR;
      auto trimChar = [&](char c) {
        return argc == 0
                   ? asciiWhitespace(static_cast<unsigned char>(c))
                   : chars.find(c) != std::string::npos;
      };
      std::size_t first = 0;
      std::size_t last = source.size();
      if (name != "rstrip")
        while (first < last && trimChar(source[first])) ++first;
      if (name != "lstrip")
        while (last > first && trimChar(source[last - 1])) --last;
      result = STRING_VAL(newString(source.substr(first, last - first)));
    } else if (name == "starts_with" || name == "ends_with" ||
               name == "contains") {
      if (!methodArity(name, argc, 1, 1))
        return InterpretResult::RUNTIME_ERROR;
      std::string needle;
      if (!methodStringArg(name, receiverSlot[1], needle))
        return InterpretResult::RUNTIME_ERROR;
      bool matches = false;
      if (name == "starts_with")
        matches = source.size() >= needle.size() &&
                  source.compare(0, needle.size(), needle) == 0;
      else if (name == "ends_with")
        matches = source.size() >= needle.size() &&
                  source.compare(source.size() - needle.size(), needle.size(),
                                 needle) == 0;
      else
        matches = source.find(needle) != std::string::npos;
      result = BOOL_VAL(matches);
    } else if (name == "replace") {
      if (!methodArity(name, argc, 2, 2))
        return InterpretResult::RUNTIME_ERROR;
      std::string needle;
      std::string replacement;
      if (!methodStringArg(name, receiverSlot[1], needle) ||
          !methodStringArg(name, receiverSlot[2], replacement))
        return InterpretResult::RUNTIME_ERROR;
      if (needle.empty()) {
        runtimeError("Method 'replace' does not accept an empty search string.");
        return InterpretResult::RUNTIME_ERROR;
      }
      std::string replaced = source;
      std::size_t pos = 0;
      while ((pos = replaced.find(needle, pos)) != std::string::npos) {
        replaced.replace(pos, needle.size(), replacement);
        pos += replacement.size();
      }
      result = STRING_VAL(newString(std::move(replaced)));
    } else if (name == "split") {
      if (!methodArity(name, argc, 0, 1))
        return InterpretResult::RUNTIME_ERROR;
      VectorObject *parts = newVector();
      if (argc == 0) {
        std::size_t pos = 0;
        while (pos < source.size()) {
          while (pos < source.size() &&
                 asciiWhitespace(static_cast<unsigned char>(source[pos])))
            ++pos;
          if (pos == source.size()) break;
          std::size_t end = pos;
          while (end < source.size() &&
                 !asciiWhitespace(static_cast<unsigned char>(source[end])))
            ++end;
          parts->elements.push_back(
              STRING_VAL(newString(source.substr(pos, end - pos))));
          pos = end;
        }
      } else {
        std::string separator;
        if (!methodStringArg(name, receiverSlot[1], separator))
          return InterpretResult::RUNTIME_ERROR;
        if (separator.empty()) {
          runtimeError("Method 'split' does not accept an empty separator.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::size_t pos = 0;
        while (true) {
          std::size_t end = source.find(separator, pos);
          parts->elements.push_back(STRING_VAL(newString(source.substr(
              pos, end == std::string::npos ? std::string::npos : end - pos))));
          if (end == std::string::npos) break;
          pos = end + separator.size();
        }
      }
      result = VECTOR_VAL(parts);
    } else {
      runtimeError("Undefined string method '%s'.", name.c_str());
      return InterpretResult::RUNTIME_ERROR;
    }

    *receiverSlot = result;
    stackTop = receiverSlot + 1;
    return InterpretResult::OK;
  }

  InterpretResult run(VTable &locals, Value *outResult = nullptr) {
    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
      printf("        ");
      for (Value *slot = stack; slot < stackTop; slot++) {
        printf("[ ");
        printValue(*slot);
        printf(" ]");
      }
      printf("\n");
      chunk->disassembleInstruction(static_cast<int>(ip - chunk->code.data()));
#endif
      std::uint8_t instruction = (*ip++);
      switch (instruction) {
      case OpCode::NEGATE: {
        UNARY_OP([](Real x) { return -x; });
        break;
      }
      case OpCode::UPLUS: {
        UNARY_OP([](Real x) { return x; });
        break;
      }
      case OpCode::EXP: {
        UNARY_OP(std::exp);
        break;
      }
      case OpCode::SIN: {
        UNARY_OP(pips::sin);
        break;
      }
      case OpCode::COS: {
        UNARY_OP(pips::cos);
        break;
      }
      case OpCode::TAN: {
        UNARY_OP(pips::tan);
        break;
      }
      case OpCode::ABS: {
        UNARY_OP(std::abs);
        break;
      }
      case OpCode::LOG: {
        UNARY_OP(std::log);
        break;
      }
      case OpCode::LOG10: {
        UNARY_OP(std::log10);
        break;
      }
      case OpCode::SIGN: {
        UNARY_OP([](Real x) { return (x > 0) - (x < 0); });
        break;
      }
      case OpCode::SQRT: {
        UNARY_OP(std::sqrt);
        break;
      }
      case OpCode::ACOS: {
        UNARY_OP(std::acos);
        break;
      }
      case OpCode::ASIN: {
        UNARY_OP(std::asin);
        break;
      }
      case OpCode::ATAN: {
        UNARY_OP(std::atan);
        break;
      }
      case OpCode::ATAN2: {
        STD_BINARY_OP(std::atan2, NUMBER_VAL);
        break;
      }
      case OpCode::MIN: {
        STD_BINARY_OP(std::min, NUMBER_VAL);
        break;
      }
      case OpCode::MAX: {
        STD_BINARY_OP(std::max, NUMBER_VAL);
        break;
      }
      case OpCode::RANGE: {
        std::uint8_t count = *ip++;
        Real start = 0.0L, stop, step = 1.0L;
        if (count == 3) {
          Value sv = pop();
          if (!IS_NUMBER(sv)) {
            runtimeError("range() step must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          step = AS_NUMBER(sv);
          Value stopV = pop();
          if (!IS_NUMBER(stopV)) {
            runtimeError("range() stop must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          stop = AS_NUMBER(stopV);
          Value startV = pop();
          if (!IS_NUMBER(startV)) {
            runtimeError("range() start must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          start = AS_NUMBER(startV);
        } else if (count == 2) {
          Value stopV = pop();
          if (!IS_NUMBER(stopV)) {
            runtimeError("range() stop must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          stop = AS_NUMBER(stopV);
          Value startV = pop();
          if (!IS_NUMBER(startV)) {
            runtimeError("range() start must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          start = AS_NUMBER(startV);
        } else {
          Value stopV = pop();
          if (!IS_NUMBER(stopV)) {
            runtimeError("range() stop must be a number.");
            return InterpretResult::RUNTIME_ERROR;
          }
          stop = AS_NUMBER(stopV);
          start = 0.0L;
        }
        if (step == 0.0L) {
          runtimeError("range() step must be non-zero.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        if (step > 0.0L) {
          for (Real v = start; v < stop; v += step) {
            vec->elements.push_back(NUMBER_VAL(v));
          }
        } else {
          for (Real v = start; v > stop; v += step) {
            vec->elements.push_back(NUMBER_VAL(v));
          }
        }
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::LINSPACE: {
        Value countV = pop();
        Value stopV  = pop();
        Value startV = pop();
        if (!IS_NUMBER(startV) || !IS_NUMBER(stopV) || !IS_NUMBER(countV)) {
          runtimeError("linspace() arguments must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::int64_t n = static_cast<std::int64_t>(AS_NUMBER(countV));
        if (n < 0) {
          runtimeError("linspace() count must be non-negative.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        if (n == 1) {
          vec->elements.push_back(startV);
        } else if (n > 1) {
          Real s = AS_NUMBER(startV);
          Real e = AS_NUMBER(stopV);
          vec->elements.reserve(static_cast<size_t>(n));
          for (std::int64_t i = 0; i < n; ++i) {
            Real v = s + static_cast<Real>(i) * (e - s) / static_cast<Real>(n - 1);
            vec->elements.push_back(NUMBER_VAL(v));
          }
        }
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::LOGSPACE: {
        Value countV = pop();
        Value stopV  = pop();
        Value startV = pop();
        if (!IS_NUMBER(startV) || !IS_NUMBER(stopV) || !IS_NUMBER(countV)) {
          runtimeError("logspace() arguments must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::int64_t n = static_cast<std::int64_t>(AS_NUMBER(countV));
        if (n < 0) {
          runtimeError("logspace() count must be non-negative.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        if (n > 0) {
          Real s = AS_NUMBER(startV);
          Real e = AS_NUMBER(stopV);
          vec->elements.reserve(static_cast<size_t>(n));
          for (std::int64_t i = 0; i < n; ++i) {
            Real exponent = (n == 1) ? s : s + static_cast<Real>(i) * (e - s) / static_cast<Real>(n - 1);
            vec->elements.push_back(NUMBER_VAL(std::exp(exponent)));
          }
        }
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::LOG10SPACE: {
        Value countV = pop();
        Value stopV  = pop();
        Value startV = pop();
        if (!IS_NUMBER(startV) || !IS_NUMBER(stopV) || !IS_NUMBER(countV)) {
          runtimeError("log10space() arguments must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::int64_t n = static_cast<std::int64_t>(AS_NUMBER(countV));
        if (n < 0) {
          runtimeError("log10space() count must be non-negative.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        if (n > 0) {
          Real s = AS_NUMBER(startV);
          Real e = AS_NUMBER(stopV);
          vec->elements.reserve(static_cast<size_t>(n));
          for (std::int64_t i = 0; i < n; ++i) {
            Real exponent = (n == 1) ? s : s + static_cast<Real>(i) * (e - s) / static_cast<Real>(n - 1);
            vec->elements.push_back(NUMBER_VAL(std::pow(static_cast<Real>(10), exponent)));
          }
        }
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::ZEROS: {
        Value countV = pop();
        if (!IS_NUMBER(countV)) {
          runtimeError("zeros() argument must be a number.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::int64_t n = static_cast<std::int64_t>(AS_NUMBER(countV));
        if (n < 0) {
          runtimeError("zeros() count must be non-negative.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        vec->elements.assign(static_cast<size_t>(n), NUMBER_VAL(0.0L));
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::ONES: {
        Value countV = pop();
        if (!IS_NUMBER(countV)) {
          runtimeError("ones() argument must be a number.");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::int64_t n = static_cast<std::int64_t>(AS_NUMBER(countV));
        if (n < 0) {
          runtimeError("ones() count must be non-negative.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *vec = newVector();
        vec->elements.assign(static_cast<size_t>(n), NUMBER_VAL(1.0L));
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::CEIL: {
        UNARY_OP(std::ceil);
        break;
      }
      case OpCode::FLOOR: {
        UNARY_OP(std::floor);
        break;
      }
      case OpCode::ENV: {
        if (!IS_STRING(peek(0))) {
          runtimeError("Operand must be a string");
          return InterpretResult::RUNTIME_ERROR;
        }
        const char *evar_ = std::getenv(AS_STRING(pop()));
        if (evar_ == nullptr) {
          runtimeError("Environment variable not defined");
          return InterpretResult::RUNTIME_ERROR;
        }
        std::string evar = evar_;
        Real num_var = Utils::Big<Real>();
        if (Utils::ConvertToNumber(evar, num_var)) {
          push(NUMBER_VAL(num_var));
        } else {
          push(STRING_VAL(newString(evar)));
        }
        break;
      }
      case OpCode::ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('+');
          if (r != InterpretResult::OK) return r;
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          Real b = AS_NUMBER(pop());
          Real a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError("Operands must be two numbers, two strings, or include a vector!");
          return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OpCode::SUBTRACT: {
        if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('-');
          if (r != InterpretResult::OK) return r;
        } else {
          BINARY_OP(NUMBER_VAL, -);
        }
        break;
      }
      case OpCode::MULTIPLY: {
        if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('*');
          if (r != InterpretResult::OK) return r;
        } else {
          BINARY_OP(NUMBER_VAL, *);
        }
        break;
      }
      case OpCode::MOD: {
        if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('%');
          if (r != InterpretResult::OK) return r;
        } else {
          MOD_OP(NUMBER_VAL);
        }
        break;
      }
      case OpCode::DIVIDE: {
        if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('/');
          if (r != InterpretResult::OK) return r;
        } else {
          BINARY_OP(NUMBER_VAL, /);
        }
        break;
      }
      case OpCode::INTDIVIDE: {
        INTDIVIDE_OP(NUMBER_VAL);
        break;
      }
      case OpCode::POW: {
        if (IS_VECTOR(peek(0)) || IS_VECTOR(peek(1))) {
          auto r = binaryArith('^');
          if (r != InterpretResult::OK) return r;
        } else {
          STD_BINARY_OP(std::pow, NUMBER_VAL);
        }
        break;
      }
      case OpCode::XOR: {
        BITWISE_OP(^);
        break;
      }
      case OpCode::BOR: {
        BITWISE_OP(|);
        break;
      }
      case OpCode::BAND: {
        BITWISE_OP(&);
        break;
      }
      case OpCode::BNOT: {
        if (!IS_INTEGRAL(peek(0))) {
          runtimeError("Operand must be an integer or boolean");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (IS_BOOL(peek(0))) {
          push(BOOL_VAL(!AS_BOOL(pop())));
        } else {
          push(NUMBER_VAL(~AS_INTEGER(pop())));
        }
        break;
      }
      case OpCode::LSHIFT: {
        BITWISE_OP(<<);
        break;
      }
      case OpCode::RSHIFT: {
        BITWISE_OP(>>);
        break;
      }
      case OpCode::NOT: {
        push(BOOL_VAL(isFalsey(pop())));
        break;
      }
      case OpCode::RETURN: {
        Value result = pop();
        if (frameCount == 0) {
          // returning from top-level script
          if (outResult != nullptr) {
            *outResult = result;
          }
          return InterpretResult::OK;
        }
        frameCount--;
        // Unwind callee's locals and args.
        stackTop = frameBase;
        // Restore caller frame.
        chunk = frames[frameCount].chunk;
        ip = frames[frameCount].ip;
        frameBase = frames[frameCount].slots;
        push(result);
        break;
      }
      case OpCode::CALL: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        std::uint8_t argc = *ip++;
        auto it = functions.find(name);
        if (it == functions.end()) {
          runtimeError("Undefined function '%s'.", name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        Function &fn = it->second;
        if (argc != fn.arity) {
          runtimeError("Expected %d arguments to function '%s' but got %d.", fn.arity, name.c_str(), argc);
          return InterpretResult::RUNTIME_ERROR;
        }
        if (frameCount == FRAMES_MAX) {
          runtimeError("Frame stack overflow.");
          return InterpretResult::RUNTIME_ERROR;
        }
        // Save caller's state into the frame slot.
        frames[frameCount].function = &fn;
        frames[frameCount].chunk = chunk;
        frames[frameCount].ip = ip;
        frames[frameCount].slots = frameBase;
        frameCount++;
        // Activate callee.
        chunk = &fn.chunk;
        ip = chunk->code.data();
        frameBase = stackTop - argc;
        break;
      }
      case OpCode::DUP: {
      // Duplicate the top value on the stack
      // This is used for method call receivers, so the receiver stays on the stack
        if (!push(peek(0)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::NEW_INSTANCE: {
        std::string className = AS_STRING(chunk->constants[(*ip++)]);
        auto cit = classes.find(className);
        if (cit == classes.end()) {
          runtimeError("Undefined class '%s'.", className.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        instances.push_back(std::make_unique<Instance>());
        Instance *inst = instances.back().get();
        inst->classDef = &cit->second;
        // Initialize declared fields to nil.
        for (const auto &f : inst->classDef->fields) {
          inst->fields[f] = NIL_VAL;
        }
        if (!push(INSTANCE_VAL(inst)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::GET_PROPERTY: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        Value recv = pop();
        if (!IS_INSTANCE(recv)) {
          runtimeError("Only instances have properties.");
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        bool declared = false;
        for (const auto &f : inst->classDef->fields) {
          if (f == name) { declared = true; break; }
        }
        if (!declared) {
          runtimeError("Class '%s' has no field '%s'.",
                       inst->classDef->name.c_str(), name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        auto fit = inst->fields.find(name);
        if (fit == inst->fields.end()) {
          if (!push(NIL_VAL))
            return InterpretResult::RUNTIME_ERROR;
        } else {
          if (!push(fit->second))
            return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OpCode::SET_PROPERTY: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        Value val = pop();
        Value recv = pop();
        if (!IS_INSTANCE(recv)) {
          runtimeError("Only instances have properties.");
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        bool declared = false;
        for (const auto &f : inst->classDef->fields) {
          if (f == name) { declared = true; break; }
        }
        if (!declared) {
          runtimeError("Class '%s' has no field '%s'.",
                       inst->classDef->name.c_str(), name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        inst->fields[name] = val;
        if (!push(val))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::HAS_ATTR: {
        Value nameVal = pop();
        Value recv = pop();
        if (!IS_INSTANCE(recv)) {
          runtimeError("hasattr: first argument must be an instance.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_STRING(nameVal)) {
          runtimeError("hasattr: second argument must be a string.");
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        std::string name = AS_STRING(nameVal);
        auto fit = inst->fields.find(name);
        if (fit == inst->fields.end()) {
          if (!push(BOOL_VAL(false)))
            return InterpretResult::RUNTIME_ERROR;
        } else {
          if (!push(BOOL_VAL(true)))
            return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OpCode::GET_ATTR: {
        Value nameVal = pop();
        Value recv = pop();
        if (!IS_INSTANCE(recv)) {
          runtimeError("getattr: first argument must be an instance.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_STRING(nameVal)) {
          runtimeError("getattr: second argument must be a string.");
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        std::string name = AS_STRING(nameVal);
        auto fit = inst->fields.find(name);
        if (fit == inst->fields.end()) {
          runtimeError("getattr: instance of class '%s' has no attribute '%s'.",
                       inst->classDef->name.c_str(), name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!push(fit->second))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::SET_ATTR: {
        Value val = pop();
        Value nameVal = pop();
        Value recv = pop();
        if (!IS_INSTANCE(recv)) {
          runtimeError("setattr: first argument must be an instance.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_STRING(nameVal)) {
          runtimeError("setattr: second argument must be a string.");
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        std::string name = AS_STRING(nameVal);
        bool declared = false;
        for (const auto &f : inst->classDef->fields) {
          if (f == name) { declared = true; break; }
        }
        if (!declared) {
          inst->classDef->fields.push_back(name);
        }
        inst->fields[name] = val;
        if (!push(val))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::STR: {
        Value v = pop();
        std::string s;
        if (!valueString(v, s)) {
          runtimeError("str: unsupported value type.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!push(STRING_VAL(newString(s))))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::CALL_METHOD:
      case OpCode::CALL_METHOD_LOCAL: {
        bool hasLocalOperand = instruction == OpCode::CALL_METHOD_LOCAL;
        std::string mname = AS_STRING(chunk->constants[(*ip++)]);
        std::uint8_t argc = *ip++;
        if (hasLocalOperand) ++ip; // Used only by the device packer.
        // Receiver sits just below the args.
        Value recv = stackTop[-1 - argc];
        if (IS_VECTOR(recv)) {
          InterpretResult result = callVectorMethod(mname, argc);
          if (result != InterpretResult::OK) return result;
          break;
        }
        if (IS_STRING(recv)) {
          InterpretResult result = callStringMethod(mname, argc);
          if (result != InterpretResult::OK) return result;
          break;
        }
        if (!IS_INSTANCE(recv)) {
          runtimeError("This value type has no method '%s'.", mname.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        Instance *inst = AS_INSTANCE(recv);
        std::string qname = inst->classDef->name + "::" + mname;
        auto it = functions.find(qname);
        if (it == functions.end()) {
          runtimeError("Undefined method '%s' on class '%s'.",
                       mname.c_str(), inst->classDef->name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        Function &fn = it->second;
        if (argc != fn.arity) {
          runtimeError("Expected %d arguments to method '%s' but got %d.",
                       fn.arity, qname.c_str(), argc);
          return InterpretResult::RUNTIME_ERROR;
        }
        if (frameCount == FRAMES_MAX) {
          runtimeError("Frame stack overflow.");
          return InterpretResult::RUNTIME_ERROR;
        }
        frames[frameCount].function = &fn;
        frames[frameCount].chunk = chunk;
        frames[frameCount].ip = ip;
        frames[frameCount].slots = frameBase;
        frameCount++;
        chunk = &fn.chunk;
        ip = chunk->code.data();
        frameBase = stackTop - argc - 1;
        break;
      }
      case OpCode::POP:
        pop();
        break;
      case OpCode::BUILD_VECTOR: {
        std::uint8_t count = *ip++;
        VectorObject *vec = newVector();
        vec->elements.reserve(count);
        // Elements were pushed in order; they sit at [stackTop-count .. stackTop-1].
        Value *first = stackTop - count;
        for (int i = 0; i < count; ++i) {
          vec->elements.push_back(first[i]);
        }
        stackTop -= count;
        if (!push(VECTOR_VAL(vec)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::GET_INDEX: {
        Value idxV = pop();
        Value vecV = pop();
        if (!IS_VECTOR(vecV)) {
          runtimeError("Indexing requires a vector.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NUMBER(idxV) && !IS_BOOL(idxV)) {
          runtimeError("Vector index must be a number.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *v = AS_VECTOR(vecV);
        size_t i;
        if (!normalizeIndex(AS_INTEGER(idxV), v->elements.size(), i))
          return InterpretResult::RUNTIME_ERROR;
        if (!push(v->elements[i]))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::SET_INDEX: {
        Value val = pop();
        Value idxV = pop();
        Value vecV = pop();
        if (!IS_VECTOR(vecV)) {
          runtimeError("Indexed assignment requires a vector.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NUMBER(idxV) && !IS_BOOL(idxV)) {
          runtimeError("Vector index must be a number.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *v = AS_VECTOR(vecV);
        size_t i;
        if (!normalizeIndex(AS_INTEGER(idxV), v->elements.size(), i))
          return InterpretResult::RUNTIME_ERROR;
        v->elements[i] = val;
        if (!push(val))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::GET_SLICE: {
        Value hiV = pop();
        Value loV = pop();
        Value vecV = pop();
        if (!IS_VECTOR(vecV)) {
          runtimeError("Slicing requires a vector.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NIL(loV) && !IS_NUMBER(loV) && !IS_BOOL(loV)) {
          runtimeError("Slice bounds must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NIL(hiV) && !IS_NUMBER(hiV) && !IS_BOOL(hiV)) {
          runtimeError("Slice bounds must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *v = AS_VECTOR(vecV);
        size_t lo, hi;
        if (!normalizeSliceBounds(loV, hiV, v->elements.size(), lo, hi))
          return InterpretResult::RUNTIME_ERROR;
        VectorObject *out = newVector();
        out->elements.reserve(hi - lo);
        for (size_t i = lo; i < hi; ++i)
          out->elements.push_back(v->elements[i]);
        if (!push(VECTOR_VAL(out)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::SET_SLICE: {
        Value rhs = pop();
        Value hiV = pop();
        Value loV = pop();
        Value vecV = pop();
        if (!IS_VECTOR(vecV)) {
          runtimeError("Slice assignment requires a vector target.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_VECTOR(rhs)) {
          runtimeError("Slice assignment right-hand side must be a vector.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NIL(loV) && !IS_NUMBER(loV) && !IS_BOOL(loV)) {
          runtimeError("Slice bounds must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!IS_NIL(hiV) && !IS_NUMBER(hiV) && !IS_BOOL(hiV)) {
          runtimeError("Slice bounds must be numbers.");
          return InterpretResult::RUNTIME_ERROR;
        }
        VectorObject *v = AS_VECTOR(vecV);
        size_t lo, hi;
        if (!normalizeSliceBounds(loV, hiV, v->elements.size(), lo, hi))
          return InterpretResult::RUNTIME_ERROR;
        const auto &src = AS_VECTOR(rhs)->elements;
        v->elements.erase(v->elements.begin() + lo,
                          v->elements.begin() + hi);
        v->elements.insert(v->elements.begin() + lo, src.begin(), src.end());
        if (!push(rhs))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::DEFINE_GLOBAL: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        globals[Utils::getKey(name.c_str())] = peek(0);
        pop();
        break;
      }
      case OpCode::SET_GLOBAL: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        auto key = Utils::getKey(name.c_str());
        // for implicit declaration change to just
        // global[key] = peek(0);
        // This disallows implict declaration (must have var)
        auto found = globals.find(key);
        if (found == globals.end()) {
          // exists
          runtimeError("Undefined variable '%s'.", name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        } else {
          globals[key] = peek(0);
        }
        break;
      }
      case OpCode::GET_GLOBAL: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        auto found = locals.find(Utils::getKey(name.c_str()));
        if (found == locals.end()) {
          found = globals.find(Utils::getKey(name.c_str()));
          if (found == globals.end()) {
            runtimeError("Undefined variable '%s'.", name.c_str());
            return InterpretResult::RUNTIME_ERROR;
          }
        }
        if (!push(found->second))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::GET_LOCAL: {
        std::uint8_t slot = *ip++;
        if (!push(frameBase[slot]))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::SET_LOCAL: {
        std::uint8_t slot = *ip++;
        frameBase[slot] = peek(0);
        break;
      }
      case OpCode::GET_OUTER: {
        // find variable by going up the call stack
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        std::uint8_t slot = *ip++;
        Value *base = nullptr;
        for (int i = frameCount - 1; i >= 0; --i) {
          if (frames[i].function && frames[i].function->name == name) {
            base = (i == frameCount - 1) ? frameBase : frames[i + 1].slots;
            break;
          }
        }
        if (base == nullptr) {
          runtimeError("Enclosing function '%s' not active on call stack.",
                       name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        if (!push(base[slot]))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::SET_OUTER: {
        std::string name = AS_STRING(chunk->constants[(*ip++)]);
        std::uint8_t slot = *ip++;
        Value *base = nullptr;
        for (int i = frameCount - 1; i >= 0; --i) {
          if (frames[i].function && frames[i].function->name == name) {
            base = (i == frameCount - 1) ? frameBase : frames[i + 1].slots;
            break;
          }
        }
        if (base == nullptr) {
          runtimeError("Enclosing function '%s' not active on call stack.",
                       name.c_str());
          return InterpretResult::RUNTIME_ERROR;
        }
        base[slot] = peek(0);
        break;
      }
      case OpCode::CONSTANT: {
        Value constant = chunk->constants[(*ip++)];
        if (!push(constant))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::NIL: {
        if (!push(NIL_VAL))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::TRUE: {
        if (!push(BOOL_VAL(true)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::FALSE: {
        if (!push(BOOL_VAL(false)))
          return InterpretResult::RUNTIME_ERROR;
        break;
      }
      case OpCode::EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OpCode::GREATER:
        BINARY_OP(BOOL_VAL, >);
        break;
      case OpCode::LESS:
        BINARY_OP(BOOL_VAL, <);
        break;
      case OpCode::PRINT: {
        printValue(pop());
        printf(" ");
        break;
      }
      case OpCode::LIST: {
        printf("Locals:\n");
        for (const auto &v : locals) {
          printf("  %s = ", v.first.c_str());
          printValue(v.second);
          printf("\n");
        }
        printf("Globals:\n");
        for (const auto &v : globals) {
          printf("  %s = ", v.first.c_str());
          printValue(v.second);
          printf("\n");
        }
        // print stack values
        printf("Stack (size %ld):\n", stackTop - stack);
        for (Value *slot = stack; slot < stackTop; slot++) {
          printf("  stack[%ld] = ", slot - stack);
          printValue(*slot);
          printf("\n");
        }
        printf("Functions:\n");
        for (const auto &f : functions) {
          // print function signature
          printf("  %s(", f.first.c_str());
          for (int i = 0; i < f.second.arity; i++) {
            printf("arg%d", i);
            if (i < f.second.arity - 1)
              printf(", ");
          }
          printf(")\n");
        }
        break;
      }
      case OpCode::LIST_GLOBALS: {
        printf("Globals:\n");
        for (const auto &v : globals) {
          printf("  %s = ", v.first.c_str());
          printValue(v.second);
          printf("\n");
        }
        break;
      }
      case OpCode::LIST_LOCALS: {
        printf("Locals:\n");
        for (const auto &v : locals) {
          printf("  %s = ", v.first.c_str());
          printValue(v.second);
          printf("\n");
        }
        break;
      }
      case OpCode::LIST_STACK: {
        printf("Stack (size %ld):\n", stackTop - stack);
        for (Value *slot = stack; slot < stackTop; slot++) {
          printf("  stack[%ld] = ", slot - stack);
          printValue(*slot);
          printf("\n");
        }
        break;
      }
      case OpCode::LIST_FUNC: {
        printf("Functions:\n");
        for (const auto &f : functions) {
          // print function signature
          printf("  %s(", f.first.c_str());
          for (int i = 0; i < f.second.arity; i++) {
            printf("arg%d", i);
            if (i < f.second.arity - 1)
              printf(", ");
          }
          printf(")\n");
        }
        break;
      }
      case OpCode::NEWLINE: {
        printf("\n");
        break;
      }
      case OpCode::JUMP_IF_FALSE: {
        std::uint16_t offset =
            (ip += 2, static_cast<std::uint16_t>((ip[-2] << 8) | ip[-1]));
        if (isFalsey(peek(0)))
          ip += offset;
        break;
      }
      case OpCode::JUMP: {
        std::uint16_t offset =
            (ip += 2, static_cast<std::uint16_t>((ip[-2] << 8) | ip[-1]));
        ip += offset;
        break;
      }
      case OpCode::LOOP: {
        std::uint16_t offset =
            (ip += 2, static_cast<std::uint16_t>((ip[-2] << 8) | ip[-1]));
        ip -= offset;
        break;
      }
      }
    }
  }

  InterpretResult interpret(const char *source, char end_line = ';') {
    Function script;
    script.name = "__main__";
    Compiler compiler(this, source, end_line);
    if (!compiler.compile(&script, &functions, &classes)) {
      return InterpretResult::COMPILE_ERROR;
    }

    chunk = &script.chunk;
    ip = script.chunk.code.data();
    frameBase = stack;
    frameCount = 0;

    VTable locals;

    return run(locals);
  }
  Value call(const std::string &name, const std::vector<Value> &args) {
    auto it = functions.find(name);
    if (it == functions.end()) {
      hostError("Undefined function '%s'.", name.c_str());
      return NIL_VAL;
    }

    Function &fn = it->second;
    if (args.size() != static_cast<size_t>(fn.arity)) {
      hostError("Expected %d arguments to function '%s' but got %zu.",
                fn.arity, name.c_str(), args.size());
      return NIL_VAL;
    }

    resetExecutionState();
    for (const auto &arg : args) {
      if (!push(arg)) {
        resetExecutionState();
        return NIL_VAL;
      }
    }

    chunk = &fn.chunk;
    ip = chunk->code.data();
    frameBase = stackTop - args.size();
    frameCount = 0;

    VTable locals;
    Value result = NIL_VAL;
    InterpretResult status = run(locals, &result);
    resetExecutionState();
    return status == InterpretResult::OK ? result : NIL_VAL;
  }
  InterpretResult interpret(const char *source, char end_line, VTable &locals) {
    Function script;
    script.name = "__main__";
    Compiler compiler(this, source, end_line);
    if (!compiler.compile(&script, &functions, &classes)) {
      return InterpretResult::COMPILE_ERROR;
    }

    chunk = &script.chunk;
    ip = script.chunk.code.data();
    frameBase = stack;
    frameCount = 0;

    return run(locals);
  }
  void repl(char end_line = ';') {
    std::string source;
    bool block = false;
    std::vector<std::string> history;
    for (;;) {
      const char *prompt = block ? "... " : ">>> ";
      std::string this_line;
      auto result = pips_readline(prompt, history);
      if (!result) {
        printf("\n");
        break;
      }
      this_line = *result;
      {
        std::string trimmed = this_line;
        if (!trimmed.empty() && trimmed.back() == '\n')
          trimmed.pop_back();
        if (!trimmed.empty() && (history.empty() || history.back() != trimmed))
          history.push_back(trimmed);
      }
      source += this_line;

      // Built-in REPL commands
      {
        std::string trimmed = source;
        auto first = trimmed.find_first_not_of(" \t\n\r\f\v");
        auto last  = trimmed.find_last_not_of(" \t\n\r\f\v");
        if (first != std::string::npos)
          trimmed = trimmed.substr(first, last - first + 1);
        if (trimmed == "clear") {
          printf("\033[2J\033[H");
          fflush(stdout);
          source.clear();
          block = false;
          continue;
        }
        if (trimmed == "exit") {
          return;
        }
        if (!trimmed.empty() && trimmed[0] == '!') {
          std::system(trimmed.c_str() + 1);
          source.clear();
          block = false;
          continue;
        }
      }

      if (this_line == "\n") {
        // empty line ends a block
        interpret(source.c_str(), end_line);
        block = false;
        source.clear();
      } else if ((this_line[this_line.find_last_not_of(" \t\n\r\f\v")] ==
                  end_line) ||
                 (this_line[this_line.find_last_not_of(" \t\n\r\f\v")] ==
                  ';')) {
        // ending a statement
        // check that we are not in a block
        if (not block) {
          char end_line_ =
              (this_line[this_line.find_last_not_of(" \t\n\r\f\v")] == ';')
                  ? ';'
                  : end_line;
          interpret(source.c_str(), end_line_);
          block = false;
          source.clear();
        }
      } else {
        block = true;
      }
      // interpret(line);
    }
  }
  void runFile(std::string path) {
    char *source = Utils::readFile(path);
    auto result = interpret(source);
    std::free(source);
    if (result == InterpretResult::COMPILE_ERROR)
      exit(65);
    if (result == InterpretResult::RUNTIME_ERROR)
      exit(70);
  }
};

inline StringObject *vmNewString(VM *vm, std::string s) {
  return vm->newString(std::move(s));
}

} // namespace pips
#endif // PIPS_VM_HPP_
