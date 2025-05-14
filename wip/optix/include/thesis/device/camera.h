#pragma once

#include "thesis/utils/preprocessor.h"

#include <vector_types.h>

namespace thesis {
namespace device {

// TODO(kacper): alignments?
struct THESIS_ALIGNMENT Camera {
    float3 eye_ = {};
    float3 pixel00_ = {};
    float3 pixel_du_ = {};
    float3 pixel_dv_ = {};
};

}  // namespace device
}  // namespace thesis
