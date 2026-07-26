// Shared kernel + host harness used by both the CUDA and HIP variants of the
// device test.

// What the test does:
//   1. On the host, compile a small pips script that defines a class with
//      data members, a few globals, a helper function `dot()` that reads
//      both the class members and a plain global, and a top-level
//      `poly(x)` that calls `dot()`, pushes into a local numeric vector, and
//      returns it.
//   2. Pack `poly` (and its transitive callee `dot`) into a flat
//      `DeviceModule`.
//   3. Copy the module's four byte/value tables to device memory and patch
//      the `DeviceModule` view to point at the device-side buffers.
//   4. Launch a kernel of N threads. Each thread constructs its own
//      `DeviceVM` (per-thread stack) and runs `poly(tid)` against the
//      uploaded module.
//   5. Copy the per-thread `DeviceValue` results back and check each one
//      against `[dot() + tid*tid, tid, bias]` computed on the host. The vector
//      is inline and contains no host or device pointer.
//
// The kernel relies only on the device subset of pips (NUMBER + BOOL + bounded
// numeric VECTOR,
// arithmetic / comparison / math intrinsics, GET_LOCAL/SET_LOCAL, CALL_ID,
// JUMP/LOOP, GET_GLOBAL_ID). No allocator, exceptions, or std::string is
// touched from the device side.

#ifndef PIPS_GPU_EXAMPLE_HPP_
#define PIPS_GPU_EXAMPLE_HPP_

// Select the runtime API based on which compiler is driving us.
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
using gpuError_t = hipError_t;
#define GPU_SUCCESS hipSuccess
#define GPU_MEMCPY_HTOD hipMemcpyHostToDevice
#define GPU_MEMCPY_DTOH hipMemcpyDeviceToHost
#define gpuMalloc hipMalloc
#define gpuFree hipFree
#define gpuMemcpy hipMemcpy
#define gpuDeviceSynchronize hipDeviceSynchronize
#define gpuGetLastError hipGetLastError
#define gpuGetErrorString hipGetErrorString
#define GPU_RUNTIME_NAME "HIP"
#elif defined(__CUDACC__)
#include <cuda_runtime.h>
using gpuError_t = cudaError_t;
#define GPU_SUCCESS cudaSuccess
#define GPU_MEMCPY_HTOD cudaMemcpyHostToDevice
#define GPU_MEMCPY_DTOH cudaMemcpyDeviceToHost
#define gpuMalloc cudaMalloc
#define gpuFree cudaFree
#define gpuMemcpy cudaMemcpy
#define gpuDeviceSynchronize cudaDeviceSynchronize
#define gpuGetLastError cudaGetLastError
#define gpuGetErrorString cudaGetErrorString
#define GPU_RUNTIME_NAME "CUDA"
#else
#include <cstdlib>
#include <cstring>
using gpuError_t = int;
constexpr gpuError_t GPU_SUCCESS = 0;
constexpr int GPU_MEMCPY_HTOD = 0;
constexpr int GPU_MEMCPY_DTOH = 1;
inline gpuError_t gpuMalloc(void **ptr, std::size_t size) {
  *ptr = std::malloc(size);
  return (*ptr != nullptr || size == 0) ? GPU_SUCCESS : 1;
}
inline gpuError_t gpuFree(void *ptr) {
  std::free(ptr);
  return GPU_SUCCESS;
}
inline gpuError_t gpuMemcpy(void *dst, const void *src, std::size_t size,
                            int kind) {
  (void)kind;
  if (size == 0)
    return GPU_SUCCESS;
  if (dst == nullptr || src == nullptr)
    return 1;
  std::memcpy(dst, src, size);
  return GPU_SUCCESS;
}
inline gpuError_t gpuDeviceSynchronize() { return GPU_SUCCESS; }
inline gpuError_t gpuGetLastError() { return GPU_SUCCESS; }
inline const char *gpuGetErrorString(gpuError_t err) {
  return err == GPU_SUCCESS ? "success" : "cpu runtime error";
}
#define GPU_RUNTIME_NAME "CPU"
#endif

// Pull in the device VM, packer, and host pips VM. Order matters only in that
// device_pack.hpp internally includes vm.hpp at the bottom; both the host
// portion (Compiler/VM/etc.) and the __host__ __device__ DeviceVM live in
// translation units compiled by nvcc/hipcc without issue, because the host
// pips code is plain host C++ and the compiler dispatches it to the host
// toolchain.
#include "pips/device/device_pack.hpp"
#include "pips/device/device_vm.hpp"
#include "pips/vm.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#define GPU_CHECK(expr)                                                        \
  do {                                                                         \
    gpuError_t _e = (expr);                                                    \
    if (_e != GPU_SUCCESS) {                                                   \
      std::fprintf(stderr, "%s error at %s:%d: %s\n", GPU_RUNTIME_NAME,        \
                   __FILE__, __LINE__, gpuGetErrorString(_e));                 \
      std::exit(2);                                                            \
    }                                                                          \
  } while (0)

// Allocate `n` bytes on the device and copy `src` into it. Returns the
// device pointer (or nullptr if `n == 0`).
template <typename T> T *upload(const T *src, std::size_t n_elems) {
  if (n_elems == 0)
    return nullptr;
  T *dev = nullptr;
  GPU_CHECK(gpuMalloc(reinterpret_cast<void **>(&dev), n_elems * sizeof(T)));
  GPU_CHECK(gpuMemcpy(dev, src, n_elems * sizeof(T), GPU_MEMCPY_HTOD));
  return dev;
}

#endif // PIPS_GPU_EXAMPLE_HPP_
