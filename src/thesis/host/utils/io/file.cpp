#include "thesis/host/utils/io.h"

#include "thesis/pch.h"

#include "internal.h"
#include "thesis/host/utils/result.h"

namespace thesis::host::utils::io::detail {

Result<std::vector<std::byte>> readFile(const std::filesystem::path& filename) {
    try {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file) {
            return make_error("Failed to open file: {}", filename.string());
        }

        const auto file_size = file.tellg();
        if (file_size <= 0) {
            return make_error("File is empty or error reading file size: {}", filename.string());
        }

        std::vector<std::byte> buffer(static_cast<size_t>(file_size));
        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), file_size);

        if (!file) {
            return make_error("Error while reading file: {}", filename.string());
        }

        return buffer;
    } catch (const std::exception& e) {
        return make_error("Exception in readFile: {}", e.what());
    }
}

}  // namespace thesis::host::utils::io::detail

namespace thesis::host::utils::io::async {

std::future<Result<std::vector<std::byte>>> readFileToBytes(const std::filesystem::path& filename) {
    return std::async(std::launch::async, [filename]() -> Result<std::vector<std::byte>> {
        return detail::readFile(filename);
    });
}

}  // namespace thesis::host::utils::io::async
