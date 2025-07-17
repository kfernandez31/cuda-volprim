#include "thesis/pch.h"

#include "thesis/host/app/config.h"
#include "thesis/host/app/logging.h"
#include "thesis/host/app/renderer.h"
#include "thesis/host/utils/result.h"

#include <optix_function_table_definition.h>  // important - do not remove or include in another file!

#include <utility>

// #include <iostream>

using namespace thesis::host;

int main(int argc, char* argv[]) {
    app::logging::init();

    auto config = try_unwrap_or_exit(app::Config::parse(argc, argv));
    app::Renderer renderer(config);
    renderer.render();

    /*
    glm::mat4 host_m =
        glm::translate(glm::vec3(10.0f, 20.0f, 30.0f)) *
        glm::rotate(glm::radians(90.0f), glm::vec3(0, 0, 1)) *
        glm::scale(glm::vec3(2.0f, 3.0f, 4.0f));

    std::cout << "host matrix:" << std::endl;
    for (int i=0; i<4; ++i) {
        for (int j=0; j<4; ++j) {
            std::cout << host_m[j][i] << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "dev matrix:" << std::endl;
    auto dev_m = geometry::toDevice(host_m);
    for (int i=0; i<3; ++i) {
        for (int j=0; j<4; ++j) {
            std::cout << dev_m[i][j] << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    auto vec = make_float3(1, 0, 0);
    std::cout << "vec that will be transformed " << vec.x << ' ' << vec.y << ' ' << vec.z <<
    std::endl;

    float3 res = dev_m.transform<true>(vec);
    std::cout << "transform<true>: " << res.x << ' ' << res.y << ' ' << res.z << std::endl;

    res = dev_m.transform<false>(vec);
    std::cout << "transform<false>: " << res.x << ' ' << res.y << ' ' << res.z << std::endl;
*/

    return 0;
}
