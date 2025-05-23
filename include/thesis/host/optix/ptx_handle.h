#pragma once

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <cstddef>
#include <string.h>
#include <string_view>

namespace thesis {
namespace optix {

class PtxHandle {
   private:
    std::string data_;

   public:
    static core::Result<PtxHandle> load(const std::filesystem::path& filename) noexcept {
        PtxHandle result;
        // result.data_ = TRY(io::readFileToString(filename)); // TODO(kacper): fix
        return result;
    }

    [[nodiscard]] std::string_view data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }
};

}  // namespace optix
}  // namespace thesis
