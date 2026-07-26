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

#ifndef PIPS_DEVICE_VECTOR_MAX
#define PIPS_DEVICE_VECTOR_MAX 8
#endif

static_assert(PIPS_DEVICE_VECTOR_MAX > 0,
              "PIPS_DEVICE_VECTOR_MAX must be positive");
static_assert(PIPS_DEVICE_VECTOR_MAX <= 255,
              "PIPS_DEVICE_VECTOR_MAX must fit in a byte");

struct DeviceVector {
  std::uint8_t length;
  DeviceReal elements[PIPS_DEVICE_VECTOR_MAX];
};

enum class DeviceValueType : std::uint8_t {
  NIL = 0,
  BOOL = 1,
  NUMBER = 2,
  VECTOR = 3,
};

struct DeviceValue {
  DeviceValueType type;
  union {
    bool b;
    DeviceReal n;
    DeviceVector vector;
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

PIPS_DEVICE_HOST_INLINE bool dv_vector(const DeviceReal *elements,
                                       std::uint32_t length,
                                       DeviceValue &out) {
  if (length > PIPS_DEVICE_VECTOR_MAX || (length > 0 && !elements))
    return false;
  DeviceValue v{};
  v.type = DeviceValueType::VECTOR;
  v.as.vector.length = static_cast<std::uint8_t>(length);
  for (std::uint32_t i = 0; i < length; ++i)
    v.as.vector.elements[i] = elements[i];
  out = v;
  return true;
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
PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_vector(const DeviceValue &v) {
  return v.type == DeviceValueType::VECTOR;
}
PIPS_DEVICE_HOST_INLINE constexpr bool dv_as_bool(const DeviceValue &v) {
  return v.as.b;
}
PIPS_DEVICE_HOST_INLINE constexpr DeviceReal dv_as_number(const DeviceValue &v) {
  return v.as.n;
}
PIPS_DEVICE_HOST_INLINE constexpr std::uint8_t
dv_vector_length(const DeviceValue &v) {
  return v.as.vector.length;
}
PIPS_DEVICE_HOST_INLINE constexpr DeviceReal
dv_vector_element(const DeviceValue &v, std::uint8_t index) {
  return v.as.vector.elements[index];
}
PIPS_DEVICE_HOST_INLINE constexpr bool dv_vector_is_valid(const DeviceValue &v) {
  return !dv_is_vector(v) || v.as.vector.length <= PIPS_DEVICE_VECTOR_MAX;
}

PIPS_DEVICE_HOST_INLINE constexpr bool dv_is_falsey(const DeviceValue &v) {
  return dv_is_nil(v) || (dv_is_bool(v) && !v.as.b);
}

static_assert(std::is_trivially_copyable<DeviceValue>::value,
              "DeviceValue must be trivially copyable for device transfer");
static_assert(std::is_standard_layout<DeviceValue>::value,
              "DeviceValue must be standard layout");
static_assert(std::is_trivially_copyable<DeviceVector>::value,
              "DeviceVector must be trivially copyable for device transfer");
static_assert(std::is_standard_layout<DeviceVector>::value,
              "DeviceVector must be standard layout");

} // namespace device
} // namespace pips

#endif // PIPS_DEVICE_DEVICE_VALUE_HPP_
