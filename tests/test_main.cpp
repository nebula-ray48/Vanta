//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#define GLFW_INCLUDE_VULKAN
#include <iostream>
#include <stdexcept>
#include <string>
#include <variant>

#include "../renderer/include/ext/glfw3.h"

#include "engine_error.h"
#include "include/render_types.h"
#include "scene/camera.h"
#include "scene/mesh.h"
#include "vulkan/render/vulkan_renderer.h"

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
        auto renderer_expected = vanta::render::VulkanRenderer::create(
            "Rey Engine Test",
            window,
            kWindowWidth,
            kWindowHeight);

        if (!renderer_expected) {
            throw std::runtime_error("レンダラー初期化エラー: " + describe_error(renderer_expected.error()));
        }

        auto renderer = std::move(renderer_expected.value());
        std::cout << "VulkanRenderer の初期化に成功しました\n";

        auto floor_data = vanta::scene::create_ground_grid(10.0f, 1.0f, 0);
        auto mesh_opt = renderer.create_mesh_from_data(floor_data);
        if (!mesh_opt) {
            std::cerr << "メッシュのGPU登録に失敗しました\n";
            return -1;
        }
        auto floor_mesh_id = *mesh_opt;

        auto cube_data = vanta::scene::create_cube(1.0f, {0.8f, 0.2f, 0.2f}, 1);
        auto cube_mesh_opt = renderer.create_mesh_from_data(cube_data);
        if (!cube_mesh_opt) {
            std::cerr << "キューブのGPU登録に失敗しました\n";
            return -1;
        }
        auto cube_mesh_id = *cube_mesh_opt;

        vanta::scene::CameraData camera{};
        vanta::scene::MouseTracker mouse_tracker{};

        uint64_t frame_count = 0;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            auto input_state = vanta::scene::poll_input(window, mouse_tracker);

            float delta_time = 0.016f; // TODO 仮のデルタタイム（後でタイマーを作ります）
            camera = vanta::scene::update_camera(camera, input_state, delta_time);

           RenderSnapshot snapshot{};
            snapshot.frame_number = frame_count++;

            snapshot.view_matrix = vanta::scene::compute_projection_matrix(camera) *
                                   vanta::scene::compute_view_matrix(camera);

            // 床のインスタンス情報を追加
            RenderInstance floor_instance{};
            floor_instance.entity_id = {0};
            floor_instance.mesh_id = floor_mesh_id;
            floor_instance.model_matrix = glm::mat4(1.0f);
            snapshot.instances.push_back(floor_instance);

            // キューブのインスタンス情報を追加
            float time = static_cast<float>(glfwGetTime());
            RenderInstance cube_instance{};
            cube_instance.entity_id = {1};
            cube_instance.mesh_id = cube_mesh_id;
            glm::mat4 cube_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f));
            cube_model = glm::rotate(cube_model, time, glm::vec3(0.0f, 1.0f, 0.0f));
            cube_instance.model_matrix = cube_model;
            snapshot.instances.push_back(cube_instance);

            if (auto draw_res = renderer.draw_frame(snapshot); !draw_res) {
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
