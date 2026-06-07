#include "entry/anyhit.cuh"
#include "entry/miss.cuh"

// __raygen__rg is provided by either the megakernel (default) or the wavefront one-bounce kernel
// (THESIS_WAVEFRONT, WAVEFRONT_PLAN.md Phase 1). anyhit + miss are shared by both — the wavefront
// bounce kernel still issues optixTrace for the primary trace and NEE/MIS shadow rays.
#ifdef THESIS_WAVEFRONT
#include "entry/wavefront.cuh"
#else
#include "entry/raygen.cuh"
#endif
