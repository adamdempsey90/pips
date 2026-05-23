// Shared kernel + host harness used by both the CUDA and HIP variants of the
// device test. This header is included by:
//   examples/device_gpu_test.cu   -> compiled with nvcc
//   examples/device_gpu_test.hip  -> compiled with hipcc
//
// What the test does:
//   1. On the host, compile a small pips script that defines a class with
//      data members, a few globals, a helper function `dot()` that reads
//      both the class members and a plain global, and a top-level
//      `poly(x)` that calls `dot()` and adds `x*x`.
//   2. Pack `poly` (and its transitive callee `dot`) into a flat
//      `DeviceModule`.
//   3. Copy the module's four byte/value tables to device memory and patch
//      the `DeviceModule` view to point at the device-side buffers.
//   4. Launch a kernel of N threads. Each thread constructs its own
//      `DeviceVM` (per-thread stack) and runs `poly(tid)` against the
//      uploaded module.
//   5. Copy the per-thread `DeviceValue` results back and check each one
//      against the expected `dot() + tid*tid` value computed on the host.
//
// The kernel relies only on the device subset of pips (NUMBER + BOOL,
// arithmetic / comparison / math intrinsics, GET_LOCAL/SET_LOCAL, CALL_ID,
// JUMP/LOOP, GET_GLOBAL_ID). No allocator, exceptions, or std::string is
// touched from the device side.

#ifndef PIPS_DEVICE_GPU_TEST_COMMON_HPP_
#define PIPS_DEVICE_GPU_TEST_COMMON_HPP_

// Select the runtime API based on which compiler is driving us.
#if defined(__HIPCC__)
  #include <hip/hip_runtime.h>
  using gpuError_t = hipError_t;
  #define GPU_SUCCESS              hipSuccess
  #define GPU_MEMCPY_HTOD          hipMemcpyHostToDevice
  #define GPU_MEMCPY_DTOH          hipMemcpyDeviceToHost
  #define gpuMalloc                hipMalloc
  #define gpuFree                  hipFree
  #define gpuMemcpy                hipMemcpy
  #define gpuDeviceSynchronize     hipDeviceSynchronize
  #define gpuGetLastError          hipGetLastError
  #define gpuGetErrorString        hipGetErrorString
  #define GPU_RUNTIME_NAME         "HIP"
#elif defined(__CUDACC__)
  #include <cuda_runtime.h>
  using gpuError_t = cudaError_t;
  #define GPU_SUCCESS              cudaSuccess
  #define GPU_MEMCPY_HTOD          cudaMemcpyHostToDevice
  #define GPU_MEMCPY_DTOH          cudaMemcpyDeviceToHost
  #define gpuMalloc                cudaMalloc
  #define gpuFree                  cudaFree
  #define gpuMemcpy                cudaMemcpy
  #define gpuDeviceSynchronize     cudaDeviceSynchronize
  #define gpuGetLastError          cudaGetLastError
  #define gpuGetErrorString        cudaGetErrorString
  #define GPU_RUNTIME_NAME         "CUDA"
#else
  #error "Compile this file with nvcc (CUDA) or hipcc (HIP)."
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

#define GPU_CHECK(expr) do {                                          \
    gpuError_t _e = (expr);                                           \
    if (_e != GPU_SUCCESS) {                                          \
      std::fprintf(stderr, "%s error at %s:%d: %s\n", GPU_RUNTIME_NAME,\
                   __FILE__, __LINE__, gpuGetErrorString(_e));        \
      std::exit(2);                                                   \
    }                                                                 \
  } while (0)

namespace pdt {

// Per-thread kernel: each thread builds a fresh DeviceVM in its own
// registers/local memory, then evaluates `poly(tid)` against the shared
// module. Results and per-thread status codes are written to the output
// arrays so the host can check them.
__global__ void run_pips_kernel(pips::device::DeviceModule mod,
                                std::uint32_t entry_id,
                                const pips::device::DeviceValue *args,
                                int n_threads,
                                pips::device::DeviceValue *results,
                                std::uint8_t *statuses) {
  int tid = blockIdx.x * blockDim.x + threadIdx.x;
  if (tid >= n_threads) return;

  pips::device::DeviceVM dvm;
  pips::device::DeviceValue arg = args[tid];
  pips::device::DeviceValue result;
  pips::device::DeviceStatus st =
      dvm.run(mod, entry_id, &arg, 1, &result);
  results[tid] = result;
  statuses[tid] = static_cast<std::uint8_t>(st);
}

// Allocate `n` bytes on the device and copy `src` into it. Returns the
// device pointer (or nullptr if `n == 0`).
template <typename T>
T *upload(const T *src, std::size_t n_elems) {
  if (n_elems == 0) return nullptr;
  T *dev = nullptr;
  GPU_CHECK(gpuMalloc(reinterpret_cast<void **>(&dev), n_elems * sizeof(T)));
  GPU_CHECK(gpuMemcpy(dev, src, n_elems * sizeof(T), GPU_MEMCPY_HTOD));
  return dev;
}

inline int run_test() {
  std::printf("[%s] device GPU test starting\n", GPU_RUNTIME_NAME);

  // 1. Build a host VM and parse a small script with classes, globals, and
  //    two functions. `poly(x)` is the entry point; the packer will pull
  //    `dot` along automatically.
  pips::VM vm;
  const char *src =
      "class Cfg { var x = 3; var y = 4; }\n"
      "var cfg = new Cfg { };\n"
      "var bias = 10;\n"
      "fn dot() { return cfg.x * cfg.x + cfg.y * cfg.y + bias; }\n"
      "fn poly(x) { return dot() + x * x; }\n";
  if (vm.interpret(src) != pips::InterpretResult::OK) {
    std::fprintf(stderr, "host interpret failed\n");
    return 1;
  }

  // 2. Pack the entry function.
  pips::device::DeviceModuleStorage store;
  std::uint32_t entry_id = 0;
  std::string err;
  if (!pips::device::pack_function(vm, "poly", store, entry_id, err)) {
    std::fprintf(stderr, "pack failed: %s\n", err.c_str());
    return 1;
  }
  pips::device::DeviceModule host_view = store.view();
  std::printf("  packed: %u funcs, %u consts, %u globals, %u code bytes\n",
              host_view.function_count, host_view.constant_count,
              host_view.global_count, host_view.code_size);

  // 3. Upload the module's four tables and patch the DeviceModule pointers.
  std::uint8_t *d_code =
      upload<std::uint8_t>(host_view.code, host_view.code_size);
  pips::device::DeviceValue *d_consts =
      upload<pips::device::DeviceValue>(host_view.constants,
                                        host_view.constant_count);
  pips::device::DeviceFunction *d_funcs =
      upload<pips::device::DeviceFunction>(host_view.functions,
                                           host_view.function_count);
  pips::device::DeviceValue *d_globals =
      upload<pips::device::DeviceValue>(host_view.globals,
                                        host_view.global_count);

  pips::device::DeviceModule dev_mod = host_view;
  dev_mod.code = d_code;
  dev_mod.constants = d_consts;
  dev_mod.functions = d_funcs;
  dev_mod.globals = d_globals;

  // 4. Prepare N per-thread arguments and output buffers.
  constexpr int N = 64;
  std::vector<pips::device::DeviceValue> h_args(N);
  std::vector<pips::device::DeviceValue> h_results(N);
  std::vector<std::uint8_t> h_status(N, 0xFF);
  for (int i = 0; i < N; ++i) {
    h_args[i] = pips::device::dv_number(
        static_cast<pips::device::DeviceReal>(i));
  }

  pips::device::DeviceValue *d_args =
      upload<pips::device::DeviceValue>(h_args.data(), N);
  pips::device::DeviceValue *d_results = nullptr;
  std::uint8_t *d_status = nullptr;
  GPU_CHECK(gpuMalloc(reinterpret_cast<void **>(&d_results),
                      N * sizeof(pips::device::DeviceValue)));
  GPU_CHECK(gpuMalloc(reinterpret_cast<void **>(&d_status),
                      N * sizeof(std::uint8_t)));

  // 5. Launch.
  const int threads_per_block = 32;
  const int blocks = (N + threads_per_block - 1) / threads_per_block;
  run_pips_kernel<<<blocks, threads_per_block>>>(
      dev_mod, entry_id, d_args, N, d_results, d_status);
  GPU_CHECK(gpuGetLastError());
  GPU_CHECK(gpuDeviceSynchronize());

  // 6. Copy results back and check.
  GPU_CHECK(gpuMemcpy(h_results.data(), d_results,
                      N * sizeof(pips::device::DeviceValue), GPU_MEMCPY_DTOH));
  GPU_CHECK(gpuMemcpy(h_status.data(), d_status, N * sizeof(std::uint8_t),
                      GPU_MEMCPY_DTOH));

  int failures = 0;
  for (int i = 0; i < N; ++i) {
    // Host-side reference: dot() == 3*3 + 4*4 + 10 == 35, poly(i) == 35 + i*i.
    double expected = 3.0 * 3.0 + 4.0 * 4.0 + 10.0
                      + static_cast<double>(i) * static_cast<double>(i);
    if (h_status[i] != static_cast<std::uint8_t>(pips::device::DeviceStatus::OK)) {
      std::fprintf(stderr, "  thread %d: status=%u\n", i,
                   static_cast<unsigned>(h_status[i]));
      ++failures;
      continue;
    }
    if (!pips::device::dv_is_number(h_results[i])) {
      std::fprintf(stderr, "  thread %d: result is not a NUMBER (type=%u)\n",
                   i, static_cast<unsigned>(h_results[i].type));
      ++failures;
      continue;
    }
    double got = static_cast<double>(pips::device::dv_as_number(h_results[i]));
    double tol = 1e-9 * (1.0 + std::abs(expected));
    if (std::abs(got - expected) > tol) {
      std::fprintf(stderr, "  thread %d: got %.17g expected %.17g\n",
                   i, got, expected);
      ++failures;
    }
  }

  // 7. Cleanup. Free order doesn't matter; we ignore errors here.
  gpuFree(d_code);
  gpuFree(d_consts);
  gpuFree(d_funcs);
  gpuFree(d_globals);
  gpuFree(d_args);
  gpuFree(d_results);
  gpuFree(d_status);

  if (failures == 0) {
    std::printf("PASS device_gpu_test [%s] (%d threads)\n",
                GPU_RUNTIME_NAME, N);
    return 0;
  }
  std::printf("FAIL device_gpu_test [%s] (%d / %d failures)\n",
              GPU_RUNTIME_NAME, failures, N);
  return 1;
}

} // namespace pdt



#endif // PIPS_DEVICE_GPU_TEST_COMMON_HPP_
