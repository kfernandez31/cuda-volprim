#pragma once

#include "thesis/host/cuda/buffer_base.h"
#include "thesis/host/utils/check.h"

#include <memory>

namespace thesis::host::cuda {

namespace detail {

struct DeviceDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFree(ptr)); }
};

}  // namespace detail

template <typename T>
struct SyncBufferPolicy {
    using device_ptr_type = std::unique_ptr<T, detail::DeviceDeleter>;
    using host_ptr_type = std::unique_ptr<T[]>;
    using ContextParam = std::monostate;

    [[nodiscard]] static device_ptr_type alloc_device(size_t count, CUcontext ctx, ContextParam) {
        Context::Guard g(ctx);
        void* raw = nullptr;
        CUDA_CHECK(cudaMalloc(&raw, count * sizeof(T)));
        return device_ptr_type(static_cast<T*>(raw), {});
    }

    [[nodiscard]] static host_ptr_type alloc_host(size_t count, ContextParam) {
        return std::make_unique<T[]>(count);
    }

    static void upload(T* dst_device, const T* src_host, size_t bytes, ContextParam) {
        CUDA_CHECK(cudaMemcpy(dst_device, src_host, bytes, cudaMemcpyHostToDevice));
    }

    static void download(T* dst_host, const T* src_device, size_t bytes, ContextParam) {
        CUDA_CHECK(cudaMemcpy(dst_host, src_device, bytes, cudaMemcpyDeviceToHost));
    }

    [[nodiscard]] static ContextParam get_context_param(const host_ptr_type&,
                                                        const device_ptr_type&) {
        return {};
    }
};

template <typename T>
class Buffer : public BufferBase<T, SyncBufferPolicy<T>> {
   public:
    using Base = BufferBase<T, SyncBufferPolicy<T>>;
    using Base::download;
    using Base::upload;

    Buffer() = default;

    Buffer(size_t count, CUcontext ctx, AllocType alloc = AllocType::OnBoth)
        : Base(count, ctx, {}, alloc) {}

    Buffer(std::span<const T> data, CUcontext ctx, AllocType alloc = AllocType::OnBoth)
        : Base(data, ctx, {}, alloc) {}
};

}  // namespace thesis::host::cuda
