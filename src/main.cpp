#include "camera.h"
#include "gaussian.h"
#include "math.h"
#include "object_list.h"
#include "sphere.h"
#include "quad.h"
#include "triangle.h"
#include "bvh.h"
#include "dbg.h"

#include <filesystem>
#include <fstream>
#include <sstream>

// vec4 bvhTriangleToVec4(const BvhTriangle& tri) {
//     return {tri.x, tri.y, tri.z, 0};
// }

// std::vector<std::shared_ptr<Object>> toTriangles(std::vector<BvhTriangle>& in) {
//     std::vector<std::shared_ptr<Object>> out;
//     out.reserve(in.size() / 3);

//     for (size_t i = 0; i < in.size(); i += 3) {
//         out.emplace_back(std::make_shared<Triangle>(
//             bvhTriangleToVec4(in[i + 0]),
//             bvhTriangleToVec4(in[i + 1]),
//             bvhTriangleToVec4(in[i + 2])
//         ));
//         out.back()->albedo = random_vec<vec3>();
//     }

//     return out;
// }

int main() {
    auto T = glm::translate(vec3(0, 0, -1));
    auto R = glm::identity<mat4>();
    // auto S = glm::scale(vec3(1.2, 0.8, 0.6));
    auto S = glm::scale(vec3(0.5));

    vec3 red(0.0, 1.0, 1.0);
    vec3 green(1.0, 0.0, 1.0);
    vec3 blue(1.0, 1.0, 0.0);

    auto world = ObjectList{
        // 0. Sanity check - only one // TODO: the Y position is the opposite of what it should be
        // TODO: debug with a triangle
        // std::make_shared<Gaussian>(red, 50.0f, glm::translate(vec3(0, -1, -1)), R, S),
        std::make_shared<Sphere>(red, 50.0f, vec3(0.25, 0.5, -1), 0.5),
        // std::make_shared<Triangle>(
        //     vec3(0, 0, -1),
        //     vec3(0, 0.5, -1),
        //     vec3(0.5, 0.5, -1),
        // ),


        // 1. Two side by side, overlap
        // std::make_shared<Gaussian>(red,  75.0f, glm::translate(vec3(-0.5, 0, -1)) , R, S),
        // std::make_shared<Gaussian>(blue, 75.0f, glm::translate(vec3(+0.5, 0, -1)), R, S),

        // 2. Two side by side, no overlap
        // std::make_shared<Gaussian>(red,  75.0f, glm::translate(vec3(-1.5, 0, -1)), R, S),
        // std::make_shared<Gaussian>(blue, 75.0f, glm::translate(vec3(+1.5, 0, -1)), R, S),

        // 3. Two lined up, overlap
        // std::make_shared<Gaussian>(red,  75.0f, glm::translate(vec3(0, 0, -0.75)), R, S),
        // std::make_shared<Gaussian>(blue, 75.0f, glm::translate(vec3(0, 0, -1.25)), R, S),

        // std::make_shared<Gaussian>(red,  75.0f, glm::translate(vec3(0, 0, -0.5)), R, S),
        // std::make_shared<Gaussian>(blue, 75.0f, glm::translate(vec3(0, 0, -1.5)), R, S),

        // 4. Two lined up, no overlap
        // std::make_shared<Gaussian>(red,  50.0f, glm::translate(vec3(0, 0, -3.25)), R, S),
        // std::make_shared<Gaussian>(blue, 60.0f, glm::translate(vec3(0, 0, -1)), R, S),

        // 5. One inside the other
        // std::make_shared<Gaussian>(red,  50.0f, glm::translate(vec3(0, 0, -1.1)), R, glm::scale(vec3(0.6))),
        // std::make_shared<Gaussian>(blue, 100.0f, glm::translate(vec3(0, 0, -1.0)), R, glm::scale(vec3(0.2))),

        // 6. Three different
    //     std::make_shared<Gaussian>(red,   50.0f, glm::translate(vec3(0, -0.433, 0)),   R, glm::scale(vec3(0.5))),
    //     std::make_shared<Gaussian>(green, 50.0f, glm::translate(vec3(-0.5, 0.25, 0)),  R, glm::scale(vec3(0.5))),
    //     std::make_shared<Gaussian>(blue,  50.0f, glm::translate(vec3(0.5, 0.25, 0)),   R, glm::scale(vec3(0.5))),
    };

    Camera::CameraSettings settings;
    settings.aspect_ratio      = 16.0 / 9.0;
    settings.image_width       = 600;
    settings.samples_per_pixel = 10;
    settings.max_depth         = 10;
    settings.vertical_fov      = 55;
    settings.lookfrom          = vec3(0, 0, 0);
    settings.lookat            = vec3(0, 0, -1);
    settings.vup               = vec3(0, 1, 0);

    auto cam = Camera(settings, "meadow_2_4k.hdr"); // TODO: get a fallback white hdr
    cam.render(world, "out.exr");
    return 0;

/*
    size_t num_frames = 60;
    float radius = 3.0f;  // Camera distance from object

    Camera::CameraSettings settings;
    settings.aspect_ratio      = 16.0 / 9.0;
    settings.image_width       = 400;
    settings.samples_per_pixel = 1;
    settings.max_depth         = 1;
    settings.vertical_fov      = 50;
    settings.lookat            = vec3(0, 0, -1);
    settings.vup               = vec3(0, 1, 0);

    std::string frames_dir = "frames";
    if (!std::filesystem::exists(frames_dir)) {
        std::filesystem::create_directory(frames_dir);
        std::clog << "Created directory: " << frames_dir << std::endl;
    }

    for (size_t i = 0; i < num_frames; ++i) {
        float angle = 2.0 * M_PI * i / num_frames;  // Full 360-degree rotation
        float x = radius * cos(angle);
        float z = radius * sin(angle) - 1.0f;  // -1 to center the orbit around the object

        settings.lookfrom = vec3(x, 0, z);  // Update camera position
        Camera cam(settings);

        // Create file name
        std::ostringstream filename;
        filename << "frames/frame_" << std::setw(3) << std::setfill('0') << i << ".ppm";

        std::ofstream out(filename.str());
        if (!out) {
            std::clog << "Error!\n" << std::endl;
            return 1;
        }

        cam.render(world, out);
        std::clog << "Rendered frame: " << filename.str() << std::endl;
    }

    std::cout << "All frames rendered. Use an external tool to create a GIF.\n";
    */
}
