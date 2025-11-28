#ifndef PIPS_CHUNK_HPP_
#define PIPS_CHUNK_HPP_

//===========================================================================
// Much of this code is based on the clox language from the book
// "Crafting Interpreters" by Robert Nystrom
// https://craftinginterpreters.com/contents.html which is available at
// https://github.com/munificent/craftinginterpreters under the MIT License.
// The code was adapted for C++ and simplified in many ways.
//===========================================================================
#include "value.hpp"
#include <vector>

namespace pips {
enum OpCode {
  CONSTANT,
  NIL,
  TRUE,
  FALSE,
  NEGATE,
  UPLUS,
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,
  INTDIVIDE,
  NOT,
  XOR,
  BOR,
  BAND,
  BNOT,
  LSHIFT,
  RSHIFT,
  EQUAL,
  GREATER,
  LESS,
  EXP,
  SIN,
  COS,
  TAN,
  ABS,
  POW,
  MOD,
  LOG,
  LOG10,
  SIGN,
  SQRT,
  ACOS,
  ASIN,
  ATAN,
  CEIL,
  FLOOR,
  ATAN2,
  MIN,
  MAX,
  PRINT,
  LIST,
  NEWLINE,
  POP,
  DEFINE_GLOBAL,
  GET_GLOBAL,
  SET_GLOBAL,
  SET_LOCAL,
  GET_LOCAL,
  JUMP_IF_FALSE,
  JUMP,
  LOOP,
  RETURN
};

template <OpCode OP>
inline constexpr bool is_ConstOp() {
  return ((OP == OpCode::CONSTANT) || (OP == OpCode::DEFINE_GLOBAL) ||
          (OP == OpCode::GET_GLOBAL) || (OP == OpCode::SET_GLOBAL));
}
template <OpCode OP>
inline constexpr bool is_ByteOp() {
  return ((OP == OpCode::SET_LOCAL) || (OP == OpCode::SET_LOCAL));
}

struct Chunk {
  std::vector<uint8_t> code;
  std::vector<Value> constants;
  std::vector<int> lines;

  Chunk() {
    code.reserve(8);
    constants.reserve(8);
    lines.reserve(8);
  }

  ~Chunk() = default;

  void write(const uint8_t byte, const int line) {
    code.push_back(byte);
    lines.push_back(line);
  }

  int addConstant(Value val) {
    constants.push_back(val);
    return constants.size() - 1;
  }
  template <OpCode OP>
  int Instruction(std::string name, int i) {

    if constexpr (!is_ConstOp<OP>()) {
      printf("%s\n", name.c_str());
      return i + 1;
    } else if constexpr (is_ByteOp<OP>()) {
      uint8_t slot = code[i + 1];
      printf("%-16s %4d\n", name, slot);
      return i + 2;
    } else {
      const auto &constant = code[i + 1];
      printf("%-16s %4d '", name.c_str(), constant);
      printValue(constants[constant]);
      printf("'\n");
      return i + 2;
    }
    return i + 1;
  }
  int jumpInstruction(const char *name, int sign, int offset) {
    uint16_t jump = static_cast<uint16_t>(code[offset + 1] << 8);
    jump |= code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
  }
  int disassembleInstruction(int i) {
    printf("%04d ", i);
    const auto &byte = code[i];
    if ((i > 0) && (lines[i] == lines[i - 1])) {
      printf("   | ");
    } else {
      printf("%4d ", lines[i]);
    }

    switch (byte) {
    case OpCode::RETURN:
      return Instruction<OpCode::RETURN>("OP_RETURN", i);
    case OpCode::NEGATE:
      return Instruction<OpCode::NEGATE>("OP_NEGATE", i);
    case OpCode::UPLUS:
      return Instruction<OpCode::UPLUS>("OP_UPLUS", i);
    case OpCode::ADD:
      return Instruction<OpCode::ADD>("OP_ADD", i);
    case OpCode::MULTIPLY:
      return Instruction<OpCode::MULTIPLY>("OP_MULTIPLY", i);
    case OpCode::DIVIDE:
      return Instruction<OpCode::DIVIDE>("OP_DIVIDE", i);
    case OpCode::CONSTANT:
      return Instruction<OpCode::CONSTANT>("OP_CONSTANT", i);
    case OpCode::NIL:
      return Instruction<OpCode::NIL>("OP_NIL", i);
    case OpCode::TRUE:
      return Instruction<OpCode::TRUE>("OP_TRUE", i);
    case OpCode::FALSE:
      return Instruction<OpCode::FALSE>("OP_FALSE", i);
    case OpCode::NOT:
      return Instruction<OpCode::NOT>("OP_NOT", i);
    case OpCode::XOR:
      return Instruction<OpCode::XOR>("OP_XOR", i);
    case OpCode::LSHIFT:
      return Instruction<OpCode::LSHIFT>("OP_LSHIFT", i);
    case OpCode::RSHIFT:
      return Instruction<OpCode::RSHIFT>("OP_RSHIFT", i);
    case OpCode::EQUAL:
      return Instruction<OpCode::EQUAL>("OP_EQUAL", i);
    case OpCode::GREATER:
      return Instruction<OpCode::GREATER>("OP_GREATER", i);
    case OpCode::LESS:
      return Instruction<OpCode::LESS>("OP_LESS", i);
    case OpCode::EXP:
      return Instruction<OpCode::EXP>("OP_EXP", i);
    case OpCode::SIN:
      return Instruction<OpCode::SIN>("OP_SIN", i);
    case OpCode::COS:
      return Instruction<OpCode::COS>("OP_COS", i);
    case OpCode::TAN:
      return Instruction<OpCode::TAN>("OP_TAN", i);
    case OpCode::MOD:
      return Instruction<OpCode::MOD>("OP_MOD", i);
    case OpCode::POW:
      return Instruction<OpCode::POW>("OP_POW", i);
    case OpCode::INTDIVIDE:
      return Instruction<OpCode::INTDIVIDE>("OP_INTDIVIDE", i);
    case OpCode::LOG:
      return Instruction<OpCode::LOG>("OP_LOG", i);
    case OpCode::LOG10:
      return Instruction<OpCode::LOG10>("OP_LOG10", i);
    case OpCode::SIGN:
      return Instruction<OpCode::SIGN>("OP_SIGN", i);
    case OpCode::SQRT:
      return Instruction<OpCode::SQRT>("OP_SQRT", i);
    case OpCode::ACOS:
      return Instruction<OpCode::ACOS>("OP_ACOS", i);
    case OpCode::ASIN:
      return Instruction<OpCode::ASIN>("OP_ASIN", i);
    case OpCode::ATAN:
      return Instruction<OpCode::ATAN>("OP_ATAN", i);
    case OpCode::ATAN2:
      return Instruction<OpCode::ATAN2>("OP_ATAN2", i);
    case OpCode::MIN:
      return Instruction<OpCode::MIN>("OP_MIN", i);
    case OpCode::MAX:
      return Instruction<OpCode::MAX>("OP_MAX", i);
    case OpCode::CEIL:
      return Instruction<OpCode::CEIL>("OP_CEIL", i);
    case OpCode::FLOOR:
      return Instruction<OpCode::FLOOR>("OP_FLOOR", i);
    case OpCode::PRINT:
      return Instruction<OpCode::PRINT>("OP_PRINT", i);
    case OpCode::LIST:
      return Instruction<OpCode::LIST>("OP_LIST", i);
    case OpCode::NEWLINE:
      return Instruction<OpCode::RETURN>("OP_NEWLINE", i);
    case OpCode::POP:
      return Instruction<OpCode::POP>("OP_POP", i);
    case OpCode::DEFINE_GLOBAL:
      return Instruction<OpCode::DEFINE_GLOBAL>("OP_DEFINE_GLOBAL", i);
    case OpCode::GET_GLOBAL:
      return Instruction<OpCode::GET_GLOBAL>("OP_GET_GLOBAL", i);
    case OpCode::SET_GLOBAL:
      return Instruction<OpCode::SET_GLOBAL>("OP_SET_GLOBAL", i);
    case OpCode::GET_LOCAL:
      return Instruction<OpCode::GET_LOCAL>("OP_GET_LOCAL", i);
    case OpCode::SET_LOCAL:
      return Instruction<OpCode::SET_LOCAL>("OP_SET_LOCAL", i);
    case OpCode::JUMP:
      return jumpInstruction("OP_JUMP", 1, i);
    case OpCode::JUMP_IF_FALSE:
      return jumpInstruction("OP_JUMP_IF_FALSE", 1, i);
    case OpCode::LOOP:
      return jumpInstruction("OP_LOOP", -1, i);
    default:
      printf("Unknown opcode ??\n");
      return i + 1;
    }
  }
  void disassemble(std::string name) {
    printf("== %s ==\n", name.c_str());
    for (int i = 0; i < code.size();) {
      i = disassembleInstruction(i);
    }
  }
};
} // namespace pips
#endif // PIPS_CHUNK_HPP_
