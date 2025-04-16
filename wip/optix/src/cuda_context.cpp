#include "thesis/cuda_context.h"

#include "thesis/check.h"

#include <cuda.h>

#include <utility>

namespace thesis {

CudaContextHandle::CudaContextHandle(int device_ordinal)
{
    CU_CHECK(cuInit(0));
    CU_CHECK(cuDeviceGet(&device_, device_ordinal));
    CU_CHECK(cuCtxCreate(&context_, 0, device_));
}

CudaContextHandle::~CudaContextHandle()
{
    if (context_ != nullptr) {
        CU_CHECK(cuCtxDestroy(context_));
    }
}

CudaContextHandle::CudaContextHandle(CudaContextHandle&& other) noexcept
    : context_(std::exchange(other.context_, nullptr))
    , device_(std::exchange(other.device_, -1))
{}

CudaContextHandle& CudaContextHandle::operator=(CudaContextHandle&& other) noexcept
{
    if (this != &other) {
        if (context_ != nullptr) {
            CU_CHECK_NOEXCEPT(cuCtxDestroy(context_));
        }
        context_ = std::exchange(other.context_, nullptr);
        device_ = std::exchange(other.device_, -1);
    }
    return *this;
}

} // namespace thesis
