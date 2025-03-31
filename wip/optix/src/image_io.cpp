#pragma once

#include "thesis/image_io.h"

#include <fstream>
#include <stdexcept>

namespace thesis {

std::string read_ptx(std::string_view filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open PTX file: " + filename);
    }

    std::string ptx(file.tellg(), '\0');
    file.seekg(0);
    file.read(ptx.data(), ptx.size());
    return ptx;
}

} // namespace thesis
