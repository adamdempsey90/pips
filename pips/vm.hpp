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

#include <cmath>
#include <cstring>
#include <iostream>
#include <stdarg.h>
#include <string>
#include <unordered_map>

#include "types.hpp"
#include "chunk.hpp"
#include "compiler.hpp"
#include "scanner.hpp"
#include "utils.hpp"
#include "value.hpp"

namespace pips {

using VTable = std::unordered_map<std::string, Value>;

enum class InterpretResult { OK, COMPILE_ERROR, RUNTIME_ERROR };
// ObjString *takeString(VM *vm, char *chars, int length);

// NOTE: The VM needs to be runnable on device and host, so limit the
//       use of data structures used (e.g., no std::vector)
//   But does the compiler also need to run on device?????

// TODO: convert this to member function of VM
#define BINARY_OP(valueType, op)                                                         \
  do {                                                                                   \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                    \
      runtimeError("Operands must be numbers.");                                         \
      return InterpretResult::RUNTIME_ERROR;                                             \
    }                                                                                    \
    Real b = AS_NUMBER(pop());                                                    \
    Real a = AS_NUMBER(pop());                                                    \
    push(valueType(a op b));                                                             \
  } while (false)

#define MOD_OP(valueType)                                                                \
  do {                                                                                   \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                    \
                                                                                         \
      runtimeError("Operands must be numbers.");                                         \
      return InterpretResult::RUNTIME_ERROR;                                             \
    }                                                                                    \
    Real b = AS_NUMBER(pop());                                                    \
    Real a = AS_NUMBER(pop());                                                    \
    push(                                                                                \
        valueType(static_cast<Real>(static_cast<int>(a) % static_cast<int>(b)))); \
  } while (false)

#define INTDIVIDE_OP(valueType)                                                          \
  do {                                                                                   \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                    \
                                                                                         \
      runtimeError("Operands must be numbers.");                                         \
      return InterpretResult::RUNTIME_ERROR;                                             \
    }                                                                                    \
    Real b = AS_NUMBER(pop());                                                    \
    Real a = AS_NUMBER(pop());                                                    \
    push(valueType(static_cast<Real>(static_cast<int>(a / b))));                  \
  } while (false)

#define POWER_OP(valueType)                                                              \
  do {                                                                                   \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {                                    \
      runtimeError("Operands must be numbers.");                                         \
      return InterpretResult::RUNTIME_ERROR;                                             \
    }                                                                                    \
    Real b = AS_NUMBER(pop());                                                    \
    Real a = AS_NUMBER(pop());                                                    \
    push(valueType(std::pow(a, b)));                                                     \
  } while (false)

struct VM {
  Chunk *chunk;
  uint8_t *ip;
  Value stack[STACK_MAX];
  Value *stackTop;

  // Table strings;
  VTable globals;

  Compiler *current;

  VM() {
    // reset the stack pointer
    stackTop = stack;
    current = nullptr;
  }
  ~VM() = default; //{ freeObjects(); }

  // void freeObject(Obj *object) {
  //   switch (object->type) {
  //   case ObjType::STRING: {
  //     ObjString *string = (ObjString *)object;
  //     FREE_ARRAY(char, string->chars, string->length + 1);
  //     FREE(ObjString, object);
  //     break;
  //   }
  //   }
  // }
  // void freeObjects() {
  //   Obj *object = objects;
  //   while (object != nullptr) {
  //     Obj *next = object->next;
  //     freeObject(object);
  //     object = next;
  //   }
  // }

  void initCompiler(Compiler *compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
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

  void push(Value val) {
    *stackTop = val;
    stackTop++;
  }
  Value pop() {
    stackTop--;
    return *stackTop;
  }
  Value peek(int dist) { return stackTop[-1 - dist]; }
  bool isFalsey(Value val) { return IS_NIL(val) || (IS_BOOL(val) && !AS_BOOL(val)); }
  void concatenate() {
    std::string a_str = (pop()).as.str;
    std::string b_str = (pop()).as.str;
    push(Value(b_str + a_str));
  }
  InterpretResult run(VTable &locals) {
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
      uint8_t instruction;
      switch (instruction = (*ip++)) {
      case OpCode::NEGATE: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      }
      case OpCode::UPLUS: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(AS_NUMBER(pop())));
        break;
      }
      case OpCode::EXP: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::exp(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::SIN: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::sin(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::COS: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::cos(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::TAN: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::tan(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::ABS: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::abs(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::LOG: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::log(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::LOG10: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::log10(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::SIGN: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL((AS_NUMBER(pop()) < 0.0 ? -1.0L : 1.0L)));
        break;
      }
      case OpCode::SQRT: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::sqrt(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::ACOS: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::acos(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::ASIN: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::asin(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::ATAN: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::atan(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::CEIL: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::ceil(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::FLOOR: {
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number");
          return InterpretResult::RUNTIME_ERROR;
        }
        push(NUMBER_VAL(std::floor(AS_NUMBER(pop()))));
        break;
      }
      case OpCode::ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          Real b = AS_NUMBER(pop());
          Real a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError("Operands must be two nuumbers or two strings!");
          return InterpretResult::RUNTIME_ERROR;
        }
        break;
      }
      case OpCode::SUBTRACT: {
        BINARY_OP(NUMBER_VAL, -);
        break;
      }
      case OpCode::MULTIPLY: {
        BINARY_OP(NUMBER_VAL, *);
        break;
      }
      case OpCode::MOD: {
        MOD_OP(NUMBER_VAL);
        break;
      }
      case OpCode::DIVIDE: {
        BINARY_OP(NUMBER_VAL, /);
        break;
      }
      case OpCode::INTDIVIDE: {
        INTDIVIDE_OP(NUMBER_VAL);
        break;
      }
      case OpCode::POW: {
        POWER_OP(NUMBER_VAL);
        break;
      }
      case OpCode::NOT: {
        push(BOOL_VAL(isFalsey(pop())));
        break;
      }
      case OpCode::RETURN: {
        return InterpretResult::OK;
        break;
      }
      case OpCode::POP:
        pop();
        break;
      case OpCode::DEFINE_GLOBAL: {
        std::string name = chunk->constants[(*ip++)].as.str;
        globals[Utils::getKey(name.c_str())] = peek(0);
        pop();
        break;
      }
      case OpCode::SET_GLOBAL: {
        std::string name = chunk->constants[(*ip++)].as.str;
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
        std::string name = chunk->constants[(*ip++)].as.str;
        auto found = locals.find(Utils::getKey(name.c_str()));
        if (found == locals.end()) {
          found = globals.find(Utils::getKey(name.c_str()));
          if (found == globals.end()) {
            runtimeError("Undefined variable '%s'.", name.c_str());
            return InterpretResult::RUNTIME_ERROR;
          }
        }
        push(found->second);
        break;
      }
      case OpCode::GET_LOCAL: {
        uint8_t slot = *ip++;
        push(stack[slot]);
        break;
      }
      case OpCode::SET_LOCAL: {
        uint8_t slot = *ip++;
        stack[slot] = peek(0);
        break;
      }
      case OpCode::CONSTANT: {
        Value constant = chunk->constants[(*ip++)];
        push(constant);
        break;
      }
      case OpCode::NIL: {
        push(NIL_VAL);
        break;
      }
      case OpCode::TRUE: {
        push(BOOL_VAL(true));
        break;
      }
      case OpCode::FALSE: {
        push(BOOL_VAL(false));
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
        printf("\n");
        break;
      }
      case OpCode::JUMP_IF_FALSE: {
        uint16_t offset = (ip += 2, static_cast<uint16_t>((ip[-2] << 8) | ip[-1]));
        if (isFalsey(peek(0))) ip += offset;
        break;
      }
      case OpCode::JUMP: {
        uint16_t offset = (ip += 2, static_cast<uint16_t>((ip[-2] << 8) | ip[-1]));
        ip += offset;
        break;
      }
      case OpCode::LOOP: {
        uint16_t offset = (ip += 2, static_cast<uint16_t>((ip[-2] << 8) | ip[-1]));
        ip -= offset;
        break;
      }
      }
    }
  }

  InterpretResult interpret(const char *source, char end_line = ';') {

    // Think about shared_ptr?
    Chunk chunk_;
    Compiler compiler(this, source, end_line);
    initCompiler(&compiler);
    compiler.set_current(current);

    // compiler.init(source);
    if (!compiler.compile(&chunk_)) {
      return InterpretResult::COMPILE_ERROR;
    }

    chunk = &chunk_;
    ip = chunk_.code.data();

    VTable locals;

    return run(locals);
  }
  InterpretResult interpret(const char *source, char end_line, VTable &locals) {

    // Think about shared_ptr?
    Chunk chunk_;
    Compiler compiler(this, source, end_line);
    initCompiler(&compiler);
    compiler.set_current(current);

    // compiler.init(source);
    if (!compiler.compile(&chunk_)) {
      return InterpretResult::COMPILE_ERROR;
    }

    chunk = &chunk_;
    ip = chunk_.code.data();

    return run(locals);
  }
  void repl(char end_line = ';') {
    // Compiler compiler(this);
    std::string source;
    bool block = false;
    for (;;) {
      char line[1024] = {};
      if (not block) {
        printf(">>> ");
      } else {
        printf("... ");
      }
      if (!std::fgets(line, sizeof(line), stdin)) {
        printf("\n");
        break;
      }
      std::string this_line = line;
      source += this_line;

      if (this_line == "\n") {
        // empty line ends a block
        interpret(source.c_str(), end_line);
        block = false;
        source.clear();
      } else if ((this_line[this_line.find_last_not_of(" \t\n\r\f\v")] == end_line) ||
                 (this_line[this_line.find_last_not_of(" \t\n\r\f\v")] == ';')) {
        // ending a statement
        // check that we are not in a block
        if (not block) {
          char end_line_ = (this_line[this_line.find_last_not_of(" \t\n\r\f\v")] == ';')
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
    // Compiler compiler(this);

    // std::FILE* file = std::fopen(path.c_str(), "r");
    // if (file == nullptr) {
    //   std::fprintf(stderr, "Could not open file \"%s\".\n", path.c_str());
    //   exit(74);
    // }
    // char line[256];
    // while (std::fgets(line, sizeof(line), file) != nullptr) {
    //   interpret(line);
    // }
    // std::fclose(file);
    char *source = Utils::readFile(path);
    printf("Input::\n%s\n", source);
    auto result = interpret(source);
    std::free(source);
    if (result == InterpretResult::COMPILE_ERROR) exit(65);
    if (result == InterpretResult::RUNTIME_ERROR) exit(70);
  }
};

// inline Obj *allocateObject(VM *vm, size_t size, ObjType type) {
//   Obj *object = (Obj *)reallocate(nullptr, 0, size);
//   object->type = type;
//   vm->addAllocate(object);
//   return object;
// }

// inline ObjString *allocateString(VM *vm, char *chars, int length) {
//   ObjString *string = ALLOCATE_OBJ(vm, ObjString, ObjType::STRING);
//   string->length = length;
//   string->chars = chars;

//   vm->strings[Utils::getKey(string->chars)] = string;
//   return string;
// }

// inline ObjString *copyString(VM *vm, const char *chars, int length) {
//   auto found = vm->strings.find(Utils::getKey(chars));
//   if (found != vm->strings.end()) {
//     // exists
//     return found->second;
//   }
//   char *heapChars = ALLOCATE(char, length + 1);
//   std::memcpy(heapChars, chars, length);
//   heapChars[length] = '\0';
//   return allocateString(vm, heapChars, length);
// }

// ObjString *takeString(VM *vm, char *chars, int length) {
//   auto found = vm->strings.find(Utils::getKey(chars));
//   if (found != vm->strings.end()) {
//     FREE_ARRAY(char, chars, length + 1);
//     return found->second;
//   }
//   return allocateString(vm, chars, length);
// }
} // namespace pips
#endif // PIPS_VM_HPP_
