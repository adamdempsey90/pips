#ifndef PIPS_DEVICE_DEVICE_CHUNK_HPP_
#define PIPS_DEVICE_DEVICE_CHUNK_HPP_

#include "device_value.hpp"

#include <cstdint>


namespace pips {
namespace device {

struct DeviceFunction {
  std::uint32_t code_offset;   // byte offset into DeviceModule::code
  std::uint32_t code_size;     // number of bytes of bytecode
  std::uint32_t const_offset;  // index into DeviceModule::constants
  std::uint16_t const_count;   // number of constants for this function
  std::uint8_t arity;          // number of declared parameters
  std::uint8_t max_stack;      // conservative upper bound for stack usage
};

struct DeviceModule {
  const std::uint8_t *code;
  const DeviceValue *constants;
  const DeviceFunction *functions;
  const DeviceValue *globals;
  std::uint32_t code_size;
  std::uint32_t constant_count;
  std::uint32_t function_count;
  std::uint32_t global_count;
};

} // namespace device
} // namespace pips

#if !defined(PIPS_DEVICE_ONLY)

#include <string>
#include <vector>

namespace pips {
namespace device {

struct DeviceModuleStorage {
  std::vector<std::uint8_t> code;
  std::vector<DeviceValue> constants;
  std::vector<DeviceFunction> functions;
  std::vector<DeviceValue> globals;

  // Host-side metadata
  std::vector<std::string> function_names;
  std::vector<std::string> global_names;

  DeviceModule view() const {
    DeviceModule m{};
    m.code = code.empty() ? nullptr : code.data();
    m.constants = constants.empty() ? nullptr : constants.data();
    m.functions = functions.empty() ? nullptr : functions.data();
    m.globals = globals.empty() ? nullptr : globals.data();
    m.code_size = static_cast<std::uint32_t>(code.size());
    m.constant_count = static_cast<std::uint32_t>(constants.size());
    m.function_count = static_cast<std::uint32_t>(functions.size());
    m.global_count = static_cast<std::uint32_t>(globals.size());
    return m;
  }
};

} // namespace device
} // namespace pips

#endif // !PIPS_DEVICE_ONLY

#endif // PIPS_DEVICE_DEVICE_CHUNK_HPP_
