#pragma once

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <string.h>
#include <string_view>

#include <cstddef>

namespace thesis {
namespace optix {

class PtxHandle {
private:
    PtxHandle() = default;
    std::string data_;

    PtxHandle(PtxHandle&&) noexcept = default;
    PtxHandle& operator=(PtxHandle&&) noexcept = default;

    PtxHandle(const PtxHandle&) = default;
    PtxHandle& operator=(const PtxHandle&) = default;

public:
    static core::Result<PtxHandle> load(const std::filesystem::path& filename) noexcept {
        PtxHandle result;
        result.data_ = TRY(io::readFileToString(filename));
        return result;
    }

    [[nodiscard]] std::string_view data() const noexcept { return data_; }
    [[nodiscard]] size_t           size() const noexcept { return data_.size(); }
};

}  // namespace optix
}  // namespace thesis
