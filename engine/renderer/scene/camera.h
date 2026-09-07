//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace vanta::scene {

    struct CameraData {
        glm::vec3 position{0.0f, 5.0f, 10.0f};
        glm::vec3 up{0.0f, 1.0f, 0.0f};

        float yaw = -90.0f;
        float pitch = 0.0f;

        float fov_degrees = 45.0f;
        float aspect_ratio = 800.0f / 600.0f;
        float near_plane = 0.1f;
        float far_plane = 100.0f;
    };

    struct InputState {
        float move_x = 0.0f; // 左右の移動 (A/Dキー)
        float move_y = 0.0f; // 上下の移動 (Q/Eキーなど)
        float move_z = 0.0f; // 前後の移動 (W/Sキー)

        float delta_yaw = 0.0f;   // マウスの横移動
        float delta_pitch = 0.0f; // マウスの縦移動
    };

    struct MouseTracker {
        double last_x = 0.0;
        double last_y = 0.0;
        bool first_mouse = true;
    };

    [[nodiscard]] inline CameraData update_camera(
        const CameraData& old_camera,
        const InputState& input,
        float delta_time
    ) noexcept {
        CameraData new_camera = old_camera;

        // 視点の角度を更新
        float mouse_sensitivity = 0.1f;
        new_camera.yaw += input.delta_yaw * mouse_sensitivity;
        new_camera.pitch += input.delta_pitch * mouse_sensitivity;

        if (new_camera.pitch > 89.0f)  new_camera.pitch = 89.0f;
        if (new_camera.pitch < -89.0f) new_camera.pitch = -89.0f;

        glm::vec3 front;
        front.x = std::cos(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
        front.y = std::sin(glm::radians(new_camera.pitch));
        front.z = std::sin(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
        front = glm::normalize(front);

        glm::vec3 right = glm::normalize(glm::cross(front, new_camera.up));
        glm::vec3 up    = glm::normalize(glm::cross(right, front));

        float move_speed = 5.0f * delta_time;
        new_camera.position += front * input.move_z * move_speed;
        new_camera.position += right * input.move_x * move_speed;
        new_camera.position += up * input.move_y * move_speed;

        return new_camera;
    }

    [[nodiscard]] inline glm::mat4 compute_view_matrix(const CameraData& camera) noexcept {
        glm::vec3 front;
        front.x = std::cos(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch));
        front.y = std::sin(glm::radians(camera.pitch));
        front.z = std::sin(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch));
        front = glm::normalize(front);

        return glm::lookAt(camera.position, camera.position + front, camera.up);
    }

    [[nodiscard]] inline glm::mat4 compute_projection_matrix(const CameraData& camera) noexcept {
        auto proj = glm::perspective(glm::radians(camera.fov_degrees), camera.aspect_ratio, camera.near_plane, camera.far_plane);
        proj[1][1] *= -1.0f;
        return proj;
    }

    [[nodiscard]] InputState poll_input(GLFWwindow* window, MouseTracker& tracker) noexcept;

    [[nodiscard]] CameraData update_camera(const CameraData& old_camera, const InputState& input, float delta_time) noexcept;

}  // namespace vanta::scene
