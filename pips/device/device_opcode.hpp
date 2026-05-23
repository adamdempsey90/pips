#ifndef PIPS_DEVICE_DEVICE_OPCODE_HPP_
#define PIPS_DEVICE_DEVICE_OPCODE_HPP_

#include <cstdint>

namespace pips {
namespace device {

// Device-side opcode set. This is a deliberately reduced subset of the host
// `pips::OpCode` enum, restricted to operations that make sense on numeric +
// bool values only, with operands rewritten to be device-friendly (function
// ids and global ids instead of name-constants).
//
// Operand layout per opcode (all multi-byte operands are big-endian):
//
//   CONSTANT      <u8 const_idx>            push function's constants[idx]
//   NIL                                       push nil
//   TRUE                                      push true
//   FALSE                                     push false
//   NEGATE                                    negate top (number only)
//   UPLUS                                     no-op for number, type-check
//   NOT                                       logical not on truthy value
//   ADD, SUB, MUL, DIV, INTDIV, MOD, POW       binary numeric ops
//   EQUAL                                     equality (numbers/bools/nil)
//   GREATER, LESS                             numeric comparison
//   XOR, BOR, BAND, BNOT, LSHIFT, RSHIFT      integer-valued bitwise
//   EXP, SIN, COS, TAN, ABS, LOG, LOG10,
//   SIGN, SQRT, ACOS, ASIN, ATAN,
//   CEIL, FLOOR                               unary math
//   ATAN2, MIN, MAX                           binary math
//   POP                                       drop top value
//   GET_LOCAL     <u8 slot>                   push frameBase[slot]
//   SET_LOCAL     <u8 slot>                   frameBase[slot] = peek(0)
//   GET_GLOBAL_ID <u16 id>                    push module.globals[id]
//   JUMP_IF_FALSE <u16 offset>                if falsey, ip += offset
//   JUMP          <u16 offset>                ip += offset
//   LOOP          <u16 offset>                ip -= offset
//   CALL_ID       <u16 func_id> <u8 argc>     call function by id
//   RETURN                                    return top of stack
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
  RETURN,
};

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_OPCODE_HPP_
