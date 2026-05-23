// CUDA build of the pips device VM test.
//
// Build:
//   nvcc -std=c++17 -O2 -I <path/to/pips-repo-root> \
//        examples/device_gpu_test.cu -o device_gpu_test_cuda
//
// Run:
//   ./device_gpu_test_cuda
//
// Optional NVCC flags worth knowing:
//   -arch=sm_70 (or your target arch) to skip JIT compilation
//   --expt-relaxed-constexpr if your nvcc version complains about constexpr
//   helpers in <cmath>; latest nvcc does not need this.
//
// The full kernel + host harness is shared with the HIP build and lives in
// device_gpu_test_common.hpp.

#include "device_gpu_test_common.hpp"
int main() { return pdt::run_test(); }