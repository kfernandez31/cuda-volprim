#include "camera.h"
#include "gaussian.h"
#include "math.h"
#include "object_list.h"
#include "sphere.h"

/*
int main() {
    auto T = glm::translate(glm::vec3(0, 0, -1));
    auto R = glm::identity<glm::mat4>();
    auto S = glm::scale(glm::vec3(1.2, 0.5, 0.5));

    glm::vec3 color(0.5);

    auto world = ObjectList{
        std::make_shared<Sphere>(color, 1.0f, glm::vec3(0, -100.5, -1), 100),
        std::make_shared<Gaussian>(color, 1.0f, T, R, S),
    };

    Camera::CameraSettings settings;
    settings.aspect_ratio      = 16.0 / 9.0;
    settings.image_width       = 400;
    settings.samples_per_pixel = 25;
    settings.max_depth         = 10;
    settings.vertical_fov      = 40;
    settings.lookfrom          = glm::vec3(1.5, 1.5, 1.5);
    settings.lookat            = glm::vec3(0, 0, -1);
    settings.vup               = glm::vec3(0, 1, 0);

    auto cam = Camera(settings);
    cam.render(world);
}
*/

namespace tinybvh
{
    using bvhint2 = glm::;
    using bvhint3 = math::int3;
    using bvhuint2 = math::uint2;
    using bvhvec2 = math::float2;
    using bvhvec3 = math::float3;
    using bvhvec4 = math::float4;
    using bvhdbl3 = math::double3;
}
#define TINYBVH_USE_CUSTOM_VECTOR_TYPE

int main() {

}