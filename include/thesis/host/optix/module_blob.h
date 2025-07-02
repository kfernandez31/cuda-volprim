#pragma once

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <cstddef>
#include <vector>
#include <filesystem>
#include <cstddef>
#include <span>

namespace thesis::host::optix {

class ModuleBlob {
   private:
    std::vector<std::byte> data_;

   public:
    static utils::Result<ModuleBlob> load(const std::filesystem::path& filename) noexcept {
        ModuleBlob result;
        TRY_ASSIGN(result.data_, utils::io::readFileToBytes(filename));
        return result;
    }

    [[nodiscard]] std::span<const std::byte> data() const noexcept { return {data_.data(), data_.size()}; }
};

}  // namespace thesis::host::optix