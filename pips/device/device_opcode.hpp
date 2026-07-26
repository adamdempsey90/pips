#ifndef PIPS_DEVICE_DEVICE_OPCODE_HPP_
#define PIPS_DEVICE_DEVICE_OPCODE_HPP_

#include <cstdint>

namespace pips {
namespace device {

// Device-side opcode set
enum class DeviceOpCode : std::uint8_t {
  CONSTANT = 0,
  NIL,
  TRUE,
  FALSE,
  NEGATE,
  UPLUS,
  NOT,
  ADD,
  SUB,
  MUL,
  DIV,
  INTDIV,
  MOD,
  POW,
  EQUAL,
  GREATER,
  LESS,
  XOR,
  BOR,
  BAND,
  BNOT,
  LSHIFT,
  RSHIFT,
  EXP,
  SIN,
  COS,
  TAN,
  ABS,
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
  POP,
  GET_LOCAL,
  SET_LOCAL,
  GET_GLOBAL_ID,
  JUMP_IF_FALSE,
  JUMP,
  LOOP,
  CALL_ID,
  BUILD_VECTOR,
  GET_INDEX,
  RETURN,
};

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_OPCODE_HPP_
