#ifndef PIPS_DEVICE_DEVICE_COMMON_HPP_
#define PIPS_DEVICE_DEVICE_COMMON_HPP_

// Qualifier used on functions that must be callable from both host code and
// CUDA/HIP device code. On plain C++ compilers it expands to nothing.
#if defined(__CUDACC__) || defined(__HIPCC__)
#define PIPS_DEVICE_HOST __host__ __device__ inline
#define PIPS_DEVICE_HOST_INLINE PIPS_DEVICE_HOST __forceinline__
#else
#define PIPS_DEVICE_HOST inline
#if defined(__GNUC__) || defined(__clang__)
#define PIPS_DEVICE_HOST_INLINE __attribute__((always_inline)) inline
#else
#define PIPS_DEVICE_HOST_INLINE PIPS_DEVICE_HOST
#endif
#endif

#endif // PIPS_DEVICE_DEVICE_COMMON_HPP_