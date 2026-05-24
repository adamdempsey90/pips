#ifndef PIPS_DEVICE_DEVICE_COMMON_HPP_
#define PIPS_DEVICE_DEVICE_COMMON_HPP_

// Qualifier used on functions that must be callable from both host code and
// CUDA/HIP device code. On plain C++ compilers it expands to nothing.
#if defined(__CUDACC__) || defined(__HIPCC__)
#define PIPS_DEVICE_HOST __host__ __device__
#else
#define PIPS_DEVICE_HOST
#endif

#endif // PIPS_DEVICE_DEVICE_COMMON_HPP_