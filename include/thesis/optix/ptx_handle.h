#pragma once

#include "thesis/utils/io.h"
#include "thesis/utils/result.h"

#include <string.h>
#include <string_view.h>

#include <cstddef>

namespace thesis {
namespace optix {

class PtxHandle {
    PtxHandle() = default;
private:
    std::string data_;

    // TODO(kacper): are these granted by default and I don't need to write = default?
    PtxHandle(PtxHandle&&) noexcept = default;
    PtxHandle(const PtxHandle&) = default;
    PtxHandle& operator=(PtxHandle&&) noexcept = default;
    PtxHandle& operator=(const PtxHandle&) = default;

public:
    static Result<PtxHandle> load(const std::filesystem::path& filename) {
        PtxHandle result;
        result.data_ = TRY(io::readFileToString(filename));
        return result;
    }

    [[nodiscard]] std::string_view data() const noexcept { return data_; }
    [[nodiscard]] size_t           size() const noexcept { return data_.size(); }

};

}  // namespace optix
}  // namespace thesis
