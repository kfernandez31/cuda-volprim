#pragma once

// DEVICE macro: only defined during device code compilation
// This is the idiomatic approach for guarding device-only code sections
#ifdef __CUDA_ARCH__
#define DEVICE
#endif

// Function qualifiers and inlining hints
#if defined(__CUDACC__) || defined(__CUDABE__)
#define THESIS_HOST __host__
#define THESIS_DEVICE __device__
#define THESIS_HOST_DEVICE THESIS_HOST THESIS_DEVICE
#define THESIS_INLINE __forceinline__
#else
#define THESIS_HOST
#define THESIS_DEVICE
#define THESIS_HOST_DEVICE
#define THESIS_INLINE inline
#endif

#define THESIS_HOSTDEVICE THESIS_HOST THESIS_DEVICE
#define THESIS_ALIGNMENT alignas(16)
