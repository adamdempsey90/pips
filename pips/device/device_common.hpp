#ifndef PIPS_DEVICE_DEVICE_COMMON_HPP_
#define PIPS_DEVICE_DEVICE_COMMON_HPP_

// Qualifiers for functions that must be callable from both host code and
// CUDA/HIP device code.
#if defined(__CUDACC__) || defined(__HIPCC__)
#define PIPS_DEVICE_HOST_INLINE __host__ __device__ inline
#define PIPS_DEVICE_HOST_FORCEINLINE __host__ __device__ __forceinline__
#else
#define PIPS_DEVICE_HOST_INLINE inline
#if defined(__GNUC__) || defined(__clang__)
#define PIPS_DEVICE_HOST_FORCEINLINE __attribute__((always_inline)) inline
#else
#define PIPS_DEVICE_HOST_FORCEINLINE inline
#endif
#endif

#endif // PIPS_DEVICE_DEVICE_COMMON_HPP_