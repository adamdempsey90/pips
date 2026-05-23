// HIP / ROCm build of the pips device VM test.
//
// Build:
//   hipcc -std=c++17 -O2 -I <path/to/pips-repo-root> \
//         examples/device_gpu_test.hip -o device_gpu_test_hip
//
// Run:
//   ./device_gpu_test_hip
//
// Notes:
//   - hipcc recognises the .hip extension as HIP source automatically. If you
//     rename this to .cpp, add `-x hip` to force HIP mode.
//   - Targeting a specific GPU arch:  --offload-arch=gfx1100  (or whatever
//     your card reports via `rocminfo`).
//
// The full kernel + host harness is shared with the CUDA build and lives in
// device_gpu_test_common.hpp.

#include "device_gpu_test_common.hpp"
int main() { return pdt::run_test(); }