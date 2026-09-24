//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

struct GLFWwindow;

namespace vanta::scene {

    enum class CameraMode {
        Orbit,
        Fly
    };

    struct CameraData {
        CameraMode mode = CameraMode::Orbit;

        // Common
        float fov_degrees = 45.0f;
        float aspect_ratio = 800.0f / 600.0f;
        float near_plane = 0.1f;
        float far_plane = 100.0f;

        // View angles
        float yaw = 90.0f;
        float pitch = 0.0f;

        // Orbit specific
        glm::vec3 target{0.0f, 0.0f, 0.0f};
        float distance = 5.0f;

        // Fly specific / Derived position for orbit
        glm::vec3 position{0.0f, 0.0f, 5.0f};
        glm::vec3 up{0.0f, 1.0f, 0.0f};
    };

    struct InputState {
        float move_x = 0.0f; // A/D
        float move_y = 0.0f; // Q/E or Space/Shift
        float move_z = 0.0f; // W/S
        
        bool is_right_mouse_down = false;
        bool is_middle_mouse_down = false;
        bool is_left_mouse_down = false;

        float delta_yaw = 0.0f;
        float delta_pitch = 0.0f;
        
        float scroll_y = 0.0f;
    };

    struct MouseTracker {
        double last_x = 0.0;
        double last_y = 0.0;
        bool first_mouse = true;
    };

    [[nodiscard]] CameraData update_camera(const CameraData& old_camera, const InputState& input, float delta_time) noexcept;
    [[nodiscard]] glm::mat4 compute_view_matrix(const CameraData& camera) noexcept;
    [[nodiscard]] glm::mat4 compute_projection_matrix(const CameraData& camera) noexcept;
    [[nodiscard]] InputState poll_input(GLFWwindow* window, MouseTracker& tracker, float scroll_y) noexcept;

}  // namespace vanta::scene
