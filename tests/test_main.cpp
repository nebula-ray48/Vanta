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

#include "imgui.h"
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
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(kWindowWidth, kWindowHeight, "Vanta - Vulkan Test", nullptr, nullptr);
    if (window == nullptr) {
        std::cerr << "ウィンドウの作成に失敗しました\n";
        glfwTerminate();
        return -1;
    }

    std::cout << "ウィンドウを作成しました。VulkanRenderer を初期化します...\n";

    static bool g_framebuffer_resized = false;
    static int g_new_width = kWindowWidth;
    static int g_new_height = kWindowHeight;

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int width, int height) {
        g_framebuffer_resized = true;
        g_new_width = width;
        g_new_height = height;
    });

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

        static bool is_fullscreen = false;
        static int windowed_x = 100, windowed_y = 100;
        static int windowed_w = kWindowWidth, windowed_h = kWindowHeight;

        auto toggle_fullscreen = [&]() {
            is_fullscreen = !is_fullscreen;
            if (is_fullscreen) {
                glfwGetWindowPos(window, &windowed_x, &windowed_y);
                glfwGetWindowSize(window, &windowed_w, &windowed_h);
                GLFWmonitor* monitor = glfwGetPrimaryMonitor();
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            } else {
                glfwSetWindowMonitor(window, nullptr, windowed_x, windowed_y, windowed_w, windowed_h, 0);
            }
        };

        uint64_t frame_count = 0;
        
        double last_time = glfwGetTime();

        static bool auto_rotate_model = true;
        static float model_rotation[3] = {90.0f, 0.0f, 0.0f}; // pitch, yaw, roll

        static bool auto_rotate_sun = true;
        static float sun_yaw = 0.0f;
        static float sun_pitch = 45.0f;

        static bool f11_pressed_last = false;

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            bool f11_pressed = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
            if (f11_pressed && !f11_pressed_last) {
                toggle_fullscreen();
            }
            f11_pressed_last = f11_pressed;

            if (g_framebuffer_resized) {
                if (g_new_width > 0 && g_new_height > 0) {
                    (void)render.resize(static_cast<uint32_t>(g_new_width), static_cast<uint32_t>(g_new_height));
                    camera.aspect_ratio = static_cast<float>(g_new_width) / static_cast<float>(g_new_height);
                }
                g_framebuffer_resized = false;
            }
            
            render.begin_imgui_frame();
            ImGui::Begin("Debug Panel");
            ImGui::Text("Average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            if (ImGui::Button(is_fullscreen ? "Exit Fullscreen (F11)" : "Enter Fullscreen (F11)")) {
                toggle_fullscreen();
            }
            ImGui::Separator();
            
            static int material_type_selection = 1; // 0 = PBR, 1 = Toon (テスト用に1にしておく)
            static float toon_threshold = 0.5f;
            static float toon_feather = 0.02f;
            static float toon_shade_color[3] = {0.25f, 0.28f, 0.45f};
            static float outline_width = 1.0f;
            static float outline_color[3] = {0.05f, 0.05f, 0.05f};
            
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::RadioButton("PBR", &material_type_selection, 0); ImGui::SameLine();
                ImGui::RadioButton("Toon", &material_type_selection, 1);

                if (material_type_selection == 1) {
                    ImGui::Separator();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Toon Settings");
                    ImGui::SliderFloat("Shadow Threshold", &toon_threshold, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Shadow Feather", &toon_feather, 0.001f, 0.2f, "%.3f");
                    ImGui::ColorEdit3("Shade Color Tint", toon_shade_color);
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Outline Settings");
                    ImGui::SliderFloat("Outline Width", &outline_width, 0.0f, 5.0f, "%.2f");
                    ImGui::ColorEdit3("Outline Color", outline_color);
                }
            }
            
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Auto Rotate Model", &auto_rotate_model);
                if (!auto_rotate_model) {
                    ImGui::SliderFloat3("Model Rotation", model_rotation, -360.0f, 360.0f);
                }
            }
            
            if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Auto Rotate Sun", &auto_rotate_sun);
                if (!auto_rotate_sun) {
                    ImGui::SliderFloat("Sun Yaw", &sun_yaw, 0.0f, 360.0f);
                    ImGui::SliderFloat("Sun Pitch", &sun_pitch, -90.0f, 90.0f);
                }
            }
            
            if (ImGui::CollapsingHeader("Camera")) {
                ImGui::Text("Position: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
                ImGui::Text("Yaw: %.2f, Pitch: %.2f", camera.yaw, camera.pitch);
            }

            if (ImGui::CollapsingHeader("Post-Processing & Bloom", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& pp = render.post_process_settings();
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "General");
                ImGui::SliderFloat("Render Scale", &pp.render_scale, 0.1f, 1.0f, "%.2f");
                ImGui::Checkbox("Enable Shadows", &pp.enable_shadows);
                ImGui::Checkbox("Enable SSAO", &pp.enable_ssao);
                ImGui::Checkbox("Enable Bloom", &pp.enable_bloom);
                ImGui::Checkbox("Enable Skybox", &pp.enable_skybox);
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.8f, 1.0f, 0.8f, 1.0f), "Environment & Exposure");
                ImGui::SliderFloat("Exposure (EV)", &pp.exposure, -3.0f, 3.0f, "%.2f");
                ImGui::SliderFloat("Skybox Brightness", &pp.skybox_intensity, 0.0f, 5.0f, "%.2f");
                ImGui::SliderFloat("IBL Intensity", &pp.ibl_intensity, 0.0f, 5.0f, "%.2f");

                ImGui::Separator();

                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Bloom / Anamorphic Streak");
                ImGui::SliderFloat("Bloom Threshold", &pp.bloom_threshold, 0.5f, 10.0f, "%.2f");
                ImGui::SliderFloat("Bloom Soft Knee", &pp.bloom_soft_knee, 0.0f, 1.0f, "%.2f");
                ImGui::SliderFloat("Streak Length", &pp.streak_length, 0.0f, 10.0f, "%.2f");
                ImGui::ColorEdit3("Streak Tint", pp.bloom_tint);
                ImGui::SliderFloat("Bloom Intensity", &pp.bloom_intensity, 0.0f, 2.0f, "%.2f");

                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Cinematic Tone & Lens");
                ImGui::SliderFloat("Chromatic Aberration", &pp.ca_strength, 0.0f, 0.01f, "%.4f");
                ImGui::SliderFloat("Saturation", &pp.saturation, 0.0f, 2.0f, "%.2f");
                ImGui::SliderFloat("Contrast", &pp.contrast, 0.5f, 2.0f, "%.2f");
                ImGui::SliderFloat("Vignette Radius", &pp.vignette_radius, 0.2f, 2.0f, "%.2f");
                ImGui::SliderFloat("Vignette Smoothness", &pp.vignette_smoothness, 0.1f, 1.5f, "%.2f");
                ImGui::SliderFloat("Film Grain", &pp.grain_amount, 0.0f, 0.05f, "%.4f");
            }
            
            ImGui::End();
            
            double current_time = glfwGetTime();
            float delta_time = static_cast<float>(current_time - last_time);
            last_time = current_time;

            auto input_state = vanta::scene::poll_input(window, mouse_tracker, static_cast<float>(g_scroll_y));
            g_scroll_y = 0.0; // Consume scroll
            
            camera = vanta::scene::update_camera(camera, input_state, delta_time);

            RenderSnapshot snapshot{};
            snapshot.frame_number = frame_count++;
            snapshot.camera_pos = camera.position;

            snapshot.view_matrix = vanta::scene::compute_view_matrix(camera);
            snapshot.proj_matrix = vanta::scene::compute_projection_matrix(camera);
            snapshot.view_proj_matrix = snapshot.proj_matrix * snapshot.view_matrix;

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

            if (auto_rotate_model) {
                model_rotation[2] += delta_time * 30.0f;
                if (model_rotation[2] > 360.0f) model_rotation[2] -= 360.0f;
            }

            for (size_t i = 0; i < helmet_nodes.size(); ++i) {
                const auto& node = helmet_nodes[i];
                RenderInstance instance{};
                instance.entity_id = { static_cast<uint32_t>(i) };
                instance.mesh_id = node.mesh_id;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(model_rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
                model = glm::rotate(model, glm::radians(model_rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f)); // yaw
                model = glm::rotate(model, glm::radians(model_rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f)); // roll
                model = glm::scale(model, glm::vec3(2.0f));
                
                instance.model_matrix = model * node.global_transform;
                
                instance.material = node.material;
                if (material_type_selection == 0) {
                    instance.material.type = MaterialType::PBR;
                } else {
                    instance.material.type = MaterialType::Toon;
                    // デフォルトのToonパラメータ
                    instance.material.toon.base_color = glm::vec4(1.0f);
                    if (instance.material.pbr.albedo_texture_id != 0) {
                        instance.material.toon.albedo_texture_id = instance.material.pbr.albedo_texture_id;
                    }
                    if (instance.material.pbr.normal_texture_id != 0) {
                        instance.material.toon.normal_texture_id = instance.material.pbr.normal_texture_id;
                        instance.material.toon.normal_scale = instance.material.pbr.normal_scale;
                    }
                    instance.material.toon.shade_color = glm::vec4(toon_shade_color[0], toon_shade_color[1], toon_shade_color[2], 1.0f);
                    instance.material.toon.threshold = toon_threshold;
                    instance.material.toon.feather = toon_feather;
                    instance.material.toon.outline_width = outline_width;
                    instance.material.toon.outline_color = glm::vec4(outline_color[0], outline_color[1], outline_color[2], 1.0f);
                }
                snapshot.instances.push_back(instance);
            }

            if (auto_rotate_sun) {
                sun_yaw += delta_time * 30.0f;
                if (sun_yaw > 360.0f) sun_yaw -= 360.0f;
            }
            float s_pitch = glm::radians(sun_pitch);
            float s_yaw = glm::radians(sun_yaw);
            snapshot.sun_direction = glm::vec3(
                std::cos(s_pitch) * std::sin(s_yaw),
                std::sin(s_pitch),
                std::cos(s_pitch) * std::cos(s_yaw)
            );

            if (auto draw_res = render.draw_frame(snapshot); !draw_res) {
                std::cerr << "描画エラー: " << describe_error(draw_res.error()) << '\n';
                break;
            }
        }

        std::cout << "メインループを終了します。リソースを破棄します...\n";
    } catch (const std::exception& e) {
        std::cerr << "致命的なエラー: " << e.what() << '\n';
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "シャットダウン完了。GPU クラッシュは発生していません。\n";
    return 0;
}
