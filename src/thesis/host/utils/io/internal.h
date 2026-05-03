#pragma once

// Internal-only helpers shared between the io/*.cpp translation units. Lives
// under src/ (not include/) so it never becomes part of the public API.

#include "thesis/host/utils/result.h"

#include <cstddef>
#include <filesystem>
#include <vector>

namespace thesis::host::utils::io::detail {

// Slurp a file into a byte buffer. The single-pass ifstream form (ate+tellg+
// read) is the recommended C++-native approach for bulk reads — see the
// readFile-strategy benchmark referenced in PR notes.
Result<std::vector<std::byte>> readFile(const std::filesystem::path& filename);

}  // namespace thesis::host::utils::io::detail
