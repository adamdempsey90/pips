#ifndef PIPS_DEVICE_DEVICE_STATUS_HPP_
#define PIPS_DEVICE_DEVICE_STATUS_HPP_

#include <cstdint>

namespace pips {
namespace device {

// Status codes returned from DeviceVM::run. No exceptions are thrown on
// the device path; the caller inspects the status code.
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
};

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_STATUS_HPP_
