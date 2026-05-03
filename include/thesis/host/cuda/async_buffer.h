#pragma once

#include "thesis/host/cuda/buffer_base.h"
#include "thesis/host/cuda/stream.h"
#include "thesis/host/utils/check.h"

#include <memory>

namespace thesis::host::cuda {

namespace detail {

struct PinnedHostDeleter {
    inline void operator()(void* ptr) const noexcept { CUDA_CHECK_NOEXCEPT(cudaFreeHost(ptr)); }
};

struct AsyncDeviceDeleter {
    std::shared_ptr<Stream> stream_;
    inline void operator()(void* ptr) const noexcept {
        CUDA_CHECK_NOEXCEPT(cudaFreeAsync(ptr, stream_->get()));
    }
};

}  // namespace detail

template <typename T>
struct AsyncBufferPolicy {
    using device_ptr_type = std::unique_ptr<T, detail::AsyncDeviceDeleter>;
    using host_ptr_type = std::unique_ptr<T, detail::PinnedHostDeleter>;
    using ContextParam = std::shared_ptr<Stream>;

    [[nodiscard]] static device_ptr_type alloc_device(size_t count, CUcontext ctx,
                                                      ContextParam stream) {
        Context::Guard g(ctx);
        void* raw = nullptr;
        CUDA_CHECK(cudaMallocAsync(&raw, count * sizeof(T), stream->get()));
        return device_ptr_type(static_cast<T*>(raw), {stream});
    }

    [[nodiscard]] static host_ptr_type alloc_host(size_t count, ContextParam,
                                                  HostHint hint = HostHint::Cacheable) {
        void* raw = nullptr;
        const unsigned int flags = (hint == HostHint::WriteCombined)
                                       ? cudaHostAllocWriteCombined
                                       : cudaHostAllocDefault;
        CUDA_CHECK(cudaHostAlloc(&raw, count * sizeof(T), flags));
        return host_ptr_type(static_cast<T*>(raw));
    }

    static void upload(T* dst_device, const T* src_host, size_t bytes, const ContextParam& stream) {
        CUDA_CHECK(
            cudaMemcpyAsync(dst_device, src_host, bytes, cudaMemcpyHostToDevice, stream->get()));
    }

    static void download(T* dst_host, const T* src_device, size_t bytes,
                         const ContextParam& stream) {
        CUDA_CHECK(
            cudaMemcpyAsync(dst_host, src_device, bytes, cudaMemcpyDeviceToHost, stream->get()));
    }

    static void memset_device(T* device_ptr, int value, size_t bytes, const ContextParam& stream) {
        CUDA_CHECK(cudaMemsetAsync(device_ptr, value, bytes, stream->get()));
    }

    [[nodiscard]] static const ContextParam& get_context_param(const host_ptr_type&,
                                                               const device_ptr_type& device_ptr) {
        return device_ptr.get_deleter().stream_;
    }
};

template <typename T>
class AsyncBuffer : public BufferBase<T, AsyncBufferPolicy<T>> {
   public:
    using Base = BufferBase<T, AsyncBufferPolicy<T>>;
    using Base::download;
    using Base::upload;

    AsyncBuffer() = default;

    AsyncBuffer(size_t count, CUcontext ctx, std::shared_ptr<Stream> stream,
                AllocType alloc = AllocType::OnBoth, HostHint host_hint = HostHint::Cacheable)
        : Base(count, ctx, std::move(stream), alloc, host_hint) {}

    AsyncBuffer(std::span<const T> data, CUcontext ctx, std::shared_ptr<Stream> stream,
                AllocType alloc = AllocType::OnBoth, HostHint host_hint = HostHint::Cacheable)
        : Base(data, ctx, std::move(stream), alloc, host_hint) {}
};

}  // namespace thesis::host::cuda
