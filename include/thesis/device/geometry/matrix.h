#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <sutil/vec_math.h>

namespace thesis {
namespace device {
namespace geometry {

class Matrix3x4 {
    float m[3][4] = {};

   public:
    struct RowProxy {
        float* row;

        THESIS_INLINE THESIS_HOST_DEVICE float& operator[](int col) noexcept { return row[col]; }

        THESIS_INLINE THESIS_HOST_DEVICE const float& operator[](int col) const noexcept {
            return row[col];
        }

        THESIS_INLINE THESIS_HOST_DEVICE float operator*(const float3& v) const noexcept {
            return row[0] * v.x + row[1] * v.y + row[2] * v.z;
        }

        THESIS_INLINE THESIS_HOST_DEVICE operator float3() const noexcept {
            return make_float3(row[0], row[1], row[2]);
        }
    };

    THESIS_INLINE THESIS_HOST_DEVICE RowProxy operator[](int row) noexcept {
        return RowProxy{m[row]};
    }

    THESIS_INLINE THESIS_HOST_DEVICE const RowProxy operator[](int row) const noexcept {
        return RowProxy{const_cast<float*>(m[row])};
    }

    template <bool WithTranslate>
    THESIS_INLINE THESIS_HOST_DEVICE float3 transform(float3 v) const noexcept {
        auto result = make_float3((*this)[0] * v, (*this)[1] * v, (*this)[2] * v);
        if constexpr (WithTranslate) {
            result += make_float3((*this)[0][3], (*this)[1][3], (*this)[2][3]);
        }
        return result;
    }
};

}  // namespace geometry
}  // namespace device
}  // namespace thesis
