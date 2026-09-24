//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

#include "../render/include/ext/glfw3.h"

#include "engine_error.h"
#include "render_types.h"
#include "world/scene/camera.h"
#include "world/scene/mesh.h"
#include "render/vulkan/render/vulkan_renderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

constexpr uint32_t kWindowWidth = 800;
constexpr uint32_t kWindowHeight = 600;

std::string describe_error(const vanta::render::EngineError& error) {
    return std::visit([]<typename T0>(const T0& err) -> std::string {
        using T = std::decay_t<T0>;
        if constexpr (requires { err.message; }) {
            return err.message;
        } else {
            return "詳細不明な Vulkan エラー";
        }
    }, error);
}

} // namespace

int main() {
    if (glfwInit() == 0) {
        std::cerr << "GLFW の初期化に失敗しました\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Vanta - Vulkan Test", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "ウィンドウの作成に失敗しました\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "ウィンドウを作成しました。VulkanRenderer を初期化します...\n";

    try {
        RendererConfig config;
        config.app_name = "Rey Engine Test";
        config.window_handle = window;
        config.window_width = kWindowWidth;
        config.window_height = kWindowHeight;
        config.material_strategy = MaterialPipelineStrategy::PBR_Toon_Hybrid; // Testing the hybrid strategy by default
        
        auto render_expected = vanta::render::VulkanRenderer::create(config);

        if (!render_expected) {
            throw std::runtime_error("レンダラー初期化エラー: " + describe_error(render_expected.error()));
        }

        auto render = std::move(render_expected.value());
        std::cout << "VulkanRenderer の初期化に成功しました\n";

        auto floor_data = vanta::scene::create_ground_grid(10.0f, 1.0f, 0);
        auto mesh_opt = render.create_mesh_from_data(floor_data);
        if (!mesh_opt) {
            std::cerr << "メッシュのGPU登録に失敗しました\n";
            return -1;
        }
        auto floor_mesh_id = *mesh_opt;

        auto cube_data = vanta::scene::create_cube(1.0f, {0.8f, 0.2f, 0.2f}, 1);
        auto cube_mesh_opt = render.create_mesh_from_data(cube_data);
        if (!cube_mesh_opt) {
            std::cerr << "キューブのGPU登録に失敗しました\n";
            return -1;
        }
        auto cube_mesh_id = *cube_mesh_opt;

        vanta::scene::CameraData camera{};
        vanta::scene::MouseTracker mouse_tracker{};
        
        static double g_scroll_y = 0.0;
        glfwSetScrollCallback(window, [](GLFWwindow*, double /*xoffset*/, double yoffset) {
            g_scroll_y = yoffset;
        });

        uint64_t frame_count = 0;
        
        double last_time = glfwGetTime();

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();
            
            double current_time = glfwGetTime();
            float delta_time = static_cast<float>(current_time - last_time);
            last_time = current_time;

            auto input_state = vanta::scene::poll_input(window, mouse_tracker, static_cast<float>(g_scroll_y));
            g_scroll_y = 0.0; // Consume scroll
            
            camera = vanta::scene::update_camera(camera, input_state, delta_time);

            RenderSnapshot snapshot{};
            snapshot.frame_number = frame_count++;
            snapshot.camera_pos = camera.position;

            snapshot.view_matrix = vanta::scene::compute_projection_matrix(camera) *
                                   vanta::scene::compute_view_matrix(camera);

            // DamagedHelmet を描画
            static bool scene_loaded = false;
            static std::vector<vanta::render::VulkanRenderer::LoadedSceneNode> helmet_nodes;
            
            if (!scene_loaded) {
                auto nodes_res = render.load_scene("assets/models/DamagedHelmet/DamagedHelmet.gltf");
                if (nodes_res) {
                    helmet_nodes = *nodes_res;
                    std::cout << "Successfully loaded DamagedHelmet.gltf\n";
                } else {
                    std::cerr << "Failed to load DamagedHelmet: " << nodes_res.error() << "\n";
                }
                scene_loaded = true;
            }

            float time = static_cast<float>(glfwGetTime());
            for (size_t i = 0; i < helmet_nodes.size(); ++i) {
                const auto& node = helmet_nodes[i];
                RenderInstance instance{};
                instance.entity_id = { static_cast<uint32_t>(i) };
                instance.mesh_id = node.mesh_id;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, time * 0.5f, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(2.0f));
                
                instance.model_matrix = model * node.global_transform;
                
                instance.material = node.material;
                snapshot.instances.push_back(instance);
            }

            snapshot.sun_direction = glm::vec3(std::cos(time * 0.5f), 1.0f, std::sin(time * 0.5f));

            if (auto draw_res = render.draw_frame(snapshot); !draw_res) {
                std::cerr << "描画エラー: " << describe_error(draw_res.error()) << '\n';
                break;
            }
        }

        std::cout << "メインループを終了します。リソースを安全に破棄します...\n";
    } catch (const std::exception& e) {
        std::cerr << "致命的なエラー: " << e.what() << '\n';
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "シャットダウン完了。GPU クラッシュは発生していません。\n";
    return 0;
}
