#pragma once

#include "thesis/host/utils/io.h"
#include "thesis/host/utils/result.h"

#include <cstddef>
#include <string.h>
#include <string_view>

namespace thesis::host::optix {

class PTX {
   private:
    std::string data_;

   public:
    static utils::Result<PTX> load(const std::filesystem::path& filename) noexcept {
        PTX result;
        TRY_ASSIGN(result.data_, utils::io::readFileToString(filename));
        return result;
    }

    [[nodiscard]] std::string_view data() const noexcept { return data_; }
    [[nodiscard]] size_t size() const noexcept { return data_.size(); }
};

}  // namespace thesis::host::optix
