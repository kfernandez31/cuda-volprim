#include "thesis/host/utils/mitsuba_parser.h"

#include "thesis/pch.h"

#include <fstream>
#include <regex> // could added to pch.h, but better yet, replace with re2 since this is slow as shit (https://github.com/google/re2)
#include <spdlog/spdlog.h>
#include <sstream>
#include <string>

namespace thesis::host::utils {

namespace {

// Parse a Python list literal like "[1.0, 2.0, 3.0]" into float3
Result<float3> parseFloat3(const std::string& str) {
    std::regex float3_regex(R"(\[([-\d.e+]+),\s*([-\d.e+]+),\s*([-\d.e+]+)\])");
    std::smatch match;

    if (!std::regex_search(str, match, float3_regex) || match.size() != 4) {
        return make_error("Failed to parse float3 from: {}", str);
    }

    try {
        float x = std::stof(match[1].str());
        float y = std::stof(match[2].str());
        float z = std::stof(match[3].str());
        return make_float3(x, y, z);
    } catch (const std::exception& e) {
        return make_error("Failed to convert float3 values: {}", e.what());
    }
}

// Extract camera configurations from Mitsuba __init__.py
Result<std::vector<MitsubaCameraConfig>> parseCameras(const std::filesystem::path& init_py_path,
                                                      size_t default_width, size_t default_height) {
    std::ifstream file(init_py_path);
    if (!file) {
        return make_error("Failed to open Mitsuba config file: {}", init_py_path.string());
    }

    std::vector<MitsubaCameraConfig> cameras;
    std::string line;
    std::string current_camera_name;
    size_t camera_width = default_width;
    size_t camera_height = default_height;

    // State machine for parsing camera blocks
    enum class State { Idle, InCamera, InLookAt };
    State state = State::Idle;

    std::string look_at_block;

    while (std::getline(file, line)) {
        // Match camera name: 'cam_XXXX': {
        std::regex cam_start_regex(R"('(cam_\d+)':\s*\{)");
        std::smatch cam_match;
        if (std::regex_search(line, cam_match, cam_start_regex)) {
            current_camera_name = cam_match[1].str();
            state = State::InCamera;
            camera_width = default_width;
            camera_height = default_height;
            continue;
        }

        if (state == State::InCamera) {
            // Match width: 'width': 900,
            std::regex width_regex(R"('width':\s*(\d+))");
            std::smatch width_match;
            if (std::regex_search(line, width_match, width_regex)) {
                camera_width = std::stoull(width_match[1].str());
                continue;
            }

            // Match height: 'height': 600,
            std::regex height_regex(R"('height':\s*(\d+))");
            std::smatch height_match;
            if (std::regex_search(line, height_match, height_regex)) {
                camera_height = std::stoull(height_match[1].str());
                continue;
            }

            // Match start of look_at: 'to_world': T().look_at(
            if (line.find("T().look_at(") != std::string::npos) {
                state = State::InLookAt;
                look_at_block.clear();
            }
        }

        if (state == State::InLookAt) {
            look_at_block += line + "\n";

            // Check if we've reached the end of look_at block
            if (line.find("),") != std::string::npos) {
                // Parse origin, target, up from accumulated block
                std::regex origin_regex(R"(origin=\[([-\d.e+]+),\s*([-\d.e+]+),\s*([-\d.e+]+)\])");
                std::regex target_regex(R"(target=\[([-\d.e+]+),\s*([-\d.e+]+),\s*([-\d.e+]+)\])");
                std::regex up_regex(R"(up=\[([-\d.e+]+),\s*([-\d.e+]+),\s*([-\d.e+]+)\])");

                std::smatch origin_match, target_match, up_match;

                bool has_origin = std::regex_search(look_at_block, origin_match, origin_regex);
                bool has_target = std::regex_search(look_at_block, target_match, target_regex);
                bool has_up = std::regex_search(look_at_block, up_match, up_regex);

                if (has_origin && has_target && has_up) {
                    auto origin = parseFloat3(origin_match[0].str());
                    auto target = parseFloat3(target_match[0].str());
                    auto up = parseFloat3(up_match[0].str());

                    if (origin && target && up) {
                        MitsubaCameraConfig cam;
                        cam.name = current_camera_name;
                        cam.origin = origin.value();
                        cam.target = target.value();
                        cam.up = up.value();
                        cam.width = camera_width;
                        cam.height = camera_height;
                        cameras.push_back(cam);

                        spdlog::debug("Parsed camera '{}': {}×{}", current_camera_name,
                                      camera_width, camera_height);
                    }
                }

                state = State::InCamera;
                look_at_block.clear();
            }
        }
    }

    if (cameras.empty()) {
        return make_error("No cameras found in Mitsuba config");
    }

    spdlog::info("Parsed {} cameras from Mitsuba config", cameras.size());
    return cameras;
}

}  // anonymous namespace

Result<MitsubaSceneConfig> parseMitsubaScene(const std::filesystem::path& init_py_path,
                                             const std::filesystem::path& args_json_path) {
    MitsubaSceneConfig config;

    // Parse args.json for default resolution and extent
    try {
        std::ifstream json_file(args_json_path);
        if (!json_file) {
            return make_error("Failed to open args.json: {}", args_json_path.string());
        }

        // Simple JSON parsing for the specific fields we need
        std::string json_content((std::istreambuf_iterator<char>(json_file)),
                                 std::istreambuf_iterator<char>());

        // Extract cam_res (default resolution)
        std::regex cam_res_regex(R"("cam_res":\s*(\d+))");
        std::smatch cam_res_match;
        if (std::regex_search(json_content, cam_res_match, cam_res_regex)) {
            config.default_width = std::stoull(cam_res_match[1].str());
            config.default_height = std::stoull(cam_res_match[1].str());
        } else {
            config.default_width = 800;
            config.default_height = 800;
        }

        // Orthographic extent (default 3.0 based on cloud scene)
        // Will try to parse from __init__.py OBJECTS config
        config.orthographic_extent = 3.0f;  // Default fallback

        spdlog::debug("Parsed args.json: default_resolution={}×{}", config.default_width,
                      config.default_height);

    } catch (const std::exception& e) {
        return make_error("Failed to parse args.json: {}", e.what());
    }

    // Parse extent from __init__.py OBJECTS section
    try {
        std::ifstream py_file(init_py_path);
        if (py_file) {
            std::string py_content((std::istreambuf_iterator<char>(py_file)),
                                   std::istreambuf_iterator<char>());

            // Look for 'extent': value in OBJECTS dictionary
            std::regex extent_regex(R"('extent':\s*([\d.e+-]+))");
            std::smatch extent_match;
            if (std::regex_search(py_content, extent_match, extent_regex)) {
                config.orthographic_extent = std::stof(extent_match[1].str());
                spdlog::debug("Parsed orthographic extent from __init__.py: {}",
                              config.orthographic_extent);
            } else {
                spdlog::debug("No extent found in __init__.py, using default: {}",
                              config.orthographic_extent);
            }
        }
    } catch (const std::exception& e) {
        spdlog::warn("Failed to parse extent from __init__.py: {}, using default: {}", e.what(),
                     config.orthographic_extent);
    }

    // Parse cameras from __init__.py
    auto cameras_result = parseCameras(init_py_path, config.default_width, config.default_height);
    if (!cameras_result) {
        return make_error("Failed to parse cameras: {}", cameras_result.error());
    }

    config.cameras = std::move(cameras_result.value());

    return config;
}

params::Camera createOrthographicCamera(const MitsubaCameraConfig& cam_config,
                                        float orthographic_extent) {
    // Mitsuba3 orthographic camera maps film to [-1,1]² (2x2 square viewport).
    // Our camera derives width = height * aspect, so to get width = 2.0:
    const float aspect_ratio =
        static_cast<float>(cam_config.width) / static_cast<float>(cam_config.height);
    const float ortho_height = 2.0f / aspect_ratio;

    return params::Camera::createOrthographic(cam_config.width, cam_config.height,
                                              cam_config.origin, cam_config.target, cam_config.up,
                                              ortho_height);
}

}  // namespace thesis::host::utils
