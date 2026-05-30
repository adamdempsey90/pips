#ifndef PIPS_DEVICE_DEVICE_VALUE_HPP_
#define PIPS_DEVICE_DEVICE_VALUE_HPP_

#include "device_common.hpp"

#include <cstdint>
#include <type_traits>

namespace pips {
namespace device {

#ifndef PIPS_DEVICE_REAL
#define PIPS_DEVICE_REAL double
#endif

using DeviceReal = PIPS_DEVICE_REAL;

enum class DeviceValueType : std::uint8_t {
  NIL = 0,
  BOOL = 1,
  NUMBER = 2,
};

struct DeviceValue {
  DeviceValueType type;
  union {
    bool b;
    DeviceReal n;
  } as;
};

PIPS_DEVICE_HOST_INLINE DeviceValue dv_nil() {
  DeviceValue v{};
  v.type = DeviceValueType::NIL;
  v.as.n = 0;
  return v;
}

PIPS_DEVICE_HOST_INLINE DeviceValue dv_bool(bool x) {
  DeviceValue v{};
  v.type = DeviceValueType::BOOL;
  v.as.b = x;
  return v;
}

PIPS_DEVICE_HOST_INLINE DeviceValue dv_number(DeviceReal x) {
  DeviceValue v{};
  v.type = DeviceValueType::NUMBER;
  v.as.n = x;
  return v;
}

PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_nil(const DeviceValue &v) {
  return v.type == DeviceValueType::NIL;
}
PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_bool(const DeviceValue &v) {
  return v.type == DeviceValueType::BOOL;
}
PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_number(const DeviceValue &v) {
  return v.type == DeviceValueType::NUMBER;
}
PIPS_DEVICE_HOST_INLINE constexpr bool dv_as_bool(const DeviceValue &v) {
  return v.as.b;
}
PIPS_DEVICE_HOST_INLINE constexpr DeviceReal dv_as_number(const DeviceValue &v) {
  return v.as.n;
}

PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_falsey(const DeviceValue &v) {
  return dv_is_nil(v) || (dv_is_bool(v) && !v.as.b);
}

static_assert(std::is_trivially_copyable<DeviceValue>::value,
              "DeviceValue must be trivially copyable for device transfer");
static_assert(std::is_standard_layout<DeviceValue>::value,
              "DeviceValue must be standard layout");

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_VALUE_HPP_
