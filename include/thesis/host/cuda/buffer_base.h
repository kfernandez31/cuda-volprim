#pragma once

#include "thesis/host/cuda/context.h"

#include <cuda_runtime.h>

#include <cstddef>
#include <span>

namespace thesis::host::cuda {

enum class AllocType { OnBoth, OnDeviceOnly };

template <typename T, typename Policy>
class BufferBase {
   protected:
    size_t count_ = 0;
    typename Policy::device_ptr_type device_ptr_;
    typename Policy::host_ptr_type host_ptr_;
    typename Policy::ContextParam context_param_;

   public:
    BufferBase() = default;

    BufferBase(size_t count, CUcontext ctx, typename Policy::ContextParam context_param,
               AllocType alloc_type = AllocType::OnBoth)
        : count_(count),
          device_ptr_(Policy::alloc_device(count, ctx, context_param)),
          host_ptr_(alloc_type == AllocType::OnBoth ? Policy::alloc_host(count, context_param)
                                                    : nullptr),
          context_param_(context_param) {}

    BufferBase(std::span<const T> data, CUcontext ctx, typename Policy::ContextParam context_param,
               AllocType alloc_type = AllocType::OnBoth)
        : BufferBase(data.size(), ctx, context_param, alloc_type) {
        if (alloc_type == AllocType::OnBoth) {
            std::memcpy(host(), data.data(), data.size_bytes());
            upload();
        } else {
            upload(data.data());
        }
    }

    [[nodiscard]] const Policy::ContextParam& get_context_param() {
        return Policy::get_context_param(host_ptr_, device_ptr_);
    }

    [[nodiscard]] size_t size() const noexcept { return count_; }
    [[nodiscard]] size_t size_in_bytes() const noexcept { return count_ * sizeof(T); }

    [[nodiscard]] T* host() noexcept { return host_ptr_.get(); }
    [[nodiscard]] const T* host() const noexcept { return host_ptr_.get(); }

    [[nodiscard]] T* device() noexcept { return device_ptr_.get(); }
    [[nodiscard]] const T* device() const noexcept { return device_ptr_.get(); }

    [[nodiscard]] CUdeviceptr cu_device_ptr() const noexcept {
        return reinterpret_cast<CUdeviceptr>(device());
    }

    [[nodiscard]] std::span<T> host_view() noexcept { return {host(), count_}; }
    [[nodiscard]] std::span<const T> host_view() const noexcept { return {host(), count_}; }

    void upload() { upload(host()); }
    void download() { download(host()); }

    void upload(const T* src) { Policy::upload(device(), src, size_in_bytes(), context_param_); }
    void download(T* dst) { Policy::download(dst, device(), size_in_bytes(), context_param_); }

    [[nodiscard]] T& operator[](size_t i) noexcept { return host()[i]; }
    [[nodiscard]] const T& operator[](size_t i) const noexcept { return host()[i]; }

    [[nodiscard]] T* begin() noexcept { return host(); }
    [[nodiscard]] T* end() noexcept { return host() + count_; }

    [[nodiscard]] const T* begin() const noexcept { return host(); }
    [[nodiscard]] const T* end() const noexcept { return host() + count_; }
};

}  // namespace thesis::host::cuda
