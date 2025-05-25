#pragma once

#include "thesis/common/utils/preprocessor.h"

#include <vector_types.h>

#include <sutil/vec_math.h>

namespace thesis {
namespace device {

class Matrix3x4 {
   private:
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

#ifdef __CUDACC__
    template <bool WithTranslate>
    THESIS_INLINE THESIS_HOST_DEVICE static float3 transform(const Matrix3x4& mat,
                                                             float3 v) noexcept {
        auto result = make_float3(mat[0] * v, mat[1] * v, mat[2] * v);
        if constexpr (WithTranslate) {
            result += make_float3(mat[0][3], mat[1][3], mat[2][3]);
        }
        return result;
    }
#endif  // __CUDACC__
};

}  // namespace device
}  // namespace thesis
