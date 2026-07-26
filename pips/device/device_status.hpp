#ifndef PIPS_DEVICE_DEVICE_STATUS_HPP_
#define PIPS_DEVICE_DEVICE_STATUS_HPP_

#include <cstdint>

namespace pips {
namespace device {

enum class DeviceStatus : std::uint8_t {
  OK = 0,
  STACK_OVERFLOW,
  STACK_UNDERFLOW,
  FRAME_OVERFLOW,
  BAD_OPCODE,
  DIV_BY_ZERO,
  TYPE_ERROR,
  BAD_FUNCTION_ID,
  BAD_GLOBAL_ID,
  BAD_CONST_ID,
  BAD_LOCAL_SLOT,
  ARITY_MISMATCH,
  INVALID_VECTOR,
  INDEX_OUT_OF_RANGE,
  VECTOR_LENGTH_MISMATCH,
};

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_STATUS_HPP_
