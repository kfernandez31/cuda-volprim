#pragma once

#if defined(__CUDACC__) || defined(__CUDABE__)
#define DEVICE
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
