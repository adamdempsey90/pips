# Calling Functions On Device

See also [Embedding from C++](embedding.md), [Examples](examples.md), and the runnable GPU examples in `examples/device_gpu_test.cu`, `examples/device_gpu_test.hip`, and `examples/device_gpu_test_common.hpp`.

## Overview

`pips` has a device-oriented execution path for a restricted subset of the VM.

The intended flow is:

1. Build a normal host-side `pips::VM`
2. Compile and run a script with `interpret(...)`
3. Pack one named function, plus every function it calls transitively, into a flat `pips::device::DeviceModule`
4. Copy that module's backing arrays to CUDA or HIP device memory
5. In a kernel, construct a per-thread `pips::device::DeviceVM` and call `run(...)`

This path exists for GPU execution, not for the full dynamic runtime. The device VM is deliberately smaller and stricter than the host VM.

## What works on device today

- Numbers and booleans
- Named function calls
- Read-only globals
- Class data members when they can be resolved through a global instance, for example `cfg.x`
- Arithmetic, comparisons, branching, and the current reduced math opcode set

## Current limitations

- Device values support only `nil`, `bool`, and `number`
- Strings, vectors, general instances, allocation, and printing are not device-runtime features
- Globals are read-only from the device packer's point of view; code that writes globals is rejected
- The entry point must be a named compiled function present in the host VM
- The packer only accepts bytecode patterns it knows how to lower into the reduced device opcode set

## Public headers

Include the device API from `pips/device/`:

```cpp
#include <pips/device/device_pack.hpp>
#include <pips/device/device_vm.hpp>
```

The main types and functions are:

- `pips::device::DeviceValue`: trivially-copyable tagged value used on device
- `pips::device::DeviceFunction`: metadata for one packed function
- `pips::device::DeviceModule`: pointer-only view of packed code/constants/functions/globals
- `pips::device::DeviceModuleStorage`: host-side owning storage for those arrays
- `pips::device::DeviceVM`: per-thread interpreter with fixed stack and frame storage
- `pips::device::DeviceStatus`: execution status returned by `DeviceVM::run(...)`
- `pips::device::pack_function(...)`: host-side packer for one named entry function

## Host-side packing

Start with a normal `VM`, compile your script, then pack the entry function:

```cpp
#include <pips/device/device_pack.hpp>
#include <pips/vm.hpp>

#include <cstdint>
#include <string>

int main() {
  pips::VM vm;

  const char *src =
      "class Cfg { var x = 3; var y = 4; }\n"
      "var cfg = new Cfg { };\n"
      "var bias = 10;\n"
      "fn dot() { return cfg.x * cfg.x + cfg.y * cfg.y + bias; }\n"
      "fn poly(x) { return dot() + x * x; }\n";

  if (vm.interpret(src) != pips::InterpretResult::OK) {
    return 1;
  }

  pips::device::DeviceModuleStorage storage;
  std::uint32_t entry_id = 0;
  std::string error;
  if (!pips::device::pack_function(vm, "poly", storage, entry_id, error)) {
    return 1;
  }

  pips::device::DeviceModule host_view = storage.view();
  (void)entry_id;
  (void)host_view;
  return 0;
}
```

### What `pack_function(...)` does

`pack_function(vm, "poly", ...)` does not pack only `poly`. It also finds every named function `poly` calls and includes those callees in the output module.

For globals:

- Plain globals such as `bias` become entries in the packed module's `globals` array
- Class-member reads such as `cfg.x` are folded into device globals when `cfg` is a global instance with host-known field values

That is why a device function can still read script-level configuration stored in globals or in class data members, while remaining allocation-free on the GPU.

## Uploading the module to CUDA or HIP

`DeviceModuleStorage` owns host memory. Before launching a kernel, allocate device buffers for each table and copy them over:

```cpp
pips::device::DeviceModule host_view = storage.view();

std::uint8_t *d_code = upload(host_view.code, host_view.code_size);
pips::device::DeviceValue *d_constants =
    upload(host_view.constants, host_view.constant_count);
pips::device::DeviceFunction *d_functions =
    upload(host_view.functions, host_view.function_count);
pips::device::DeviceValue *d_globals =
    upload(host_view.globals, host_view.global_count);

pips::device::DeviceModule device_view = host_view;
device_view.code = d_code;
device_view.constants = d_constants;
device_view.functions = d_functions;
device_view.globals = d_globals;
```

The exact `upload(...)` helper is runtime-specific, but the pattern is always the same:

1. Allocate one device buffer per table
2. Copy the table contents from host to device
3. Patch the `DeviceModule` pointers so they point at device memory, not host memory

Passing a `DeviceModule` whose pointers still refer to host memory into a GPU kernel is invalid.

## Running a packed function in a kernel

Inside the kernel, create one `DeviceVM` per thread and call `run(...)`:

```cpp
__global__ void run_kernel(pips::device::DeviceModule module,
                           std::uint32_t entry_id,
                           const pips::device::DeviceValue *args,
                           pips::device::DeviceValue *results,
                           std::uint8_t *statuses,
                           int n) {
  int tid = blockIdx.x * blockDim.x + threadIdx.x;
  if (tid >= n) return;

  pips::device::DeviceVM vm;
  pips::device::DeviceValue result;
  pips::device::DeviceStatus st =
      vm.run(module, entry_id, &args[tid], 1, &result);

  results[tid] = result;
  statuses[tid] = static_cast<std::uint8_t>(st);
}
```

`DeviceVM::run(...)` takes:

- `module`: the packed device module
- `entry_id`: function id returned by `pack_function(...)`
- `args`: pointer to the argument list for this call
- `argc`: number of arguments
- `out_result`: where the returned `DeviceValue` is written on success

It returns a `DeviceStatus`, for example:

- `OK`
- `ARITY_MISMATCH`
- `BAD_FUNCTION_ID`
- `BAD_GLOBAL_ID`
- `TYPE_ERROR`
- `DIV_BY_ZERO`
- `STACK_OVERFLOW`

Each thread should use its own `DeviceVM`. The stack and frame arrays live inside that `DeviceVM` object.

## Checking results on the host

After the kernel finishes:

1. Copy the result array back to host memory
2. Copy the status array back to host memory
3. Verify every thread returned `DeviceStatus::OK`
4. Decode the returned `DeviceValue` with `dv_is_number(...)`, `dv_as_number(...)`, and related helpers

This is the pattern used by the repository's standalone GPU examples.

## CUDA and HIP example programs

The repository includes one minimal program per runtime:

- `examples/device_gpu_test.cu`
- `examples/device_gpu_test.hip`

Both include the shared harness in `examples/device_gpu_test_common.hpp`.

Example compile commands:

```bash
nvcc -std=c++17 -O2 -I . examples/device_gpu_test.cu -o device_gpu_test_cuda
./device_gpu_test_cuda
```

```bash
hipcc -std=c++17 -O2 -I . examples/device_gpu_test.hip -o device_gpu_test_hip
./device_gpu_test_hip
```

Useful target flags when compiling on a real GPU machine:

- CUDA: `-arch=sm_80` or whatever matches your GPU
- HIP: `--offload-arch=gfx1100` or the architecture reported by `rocminfo`

## Practical guidance

- Keep the device entry function numeric and explicit
- Treat globals as read-only configuration
- Use host code to allocate and upload the packed tables
- Construct a fresh `DeviceVM` inside each thread that executes `run(...)`
- Check `DeviceStatus` before decoding the returned value
- Start from `examples/device_gpu_test_common.hpp` if you want a working template