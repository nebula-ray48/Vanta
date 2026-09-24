#include <GLFW/glfw3.h>
#include "world/scene/camera.h"
#include <algorithm>

namespace vanta::scene {

[[nodiscard]] CameraData update_camera(
    const CameraData& old_camera,
    const InputState& input,
    float delta_time
) noexcept {
    CameraData new_camera = old_camera;

    float mouse_sensitivity = 0.2f;
    float pan_speed = 0.01f * new_camera.distance;
    float zoom_speed = 0.5f;

    if (new_camera.mode == CameraMode::Orbit) {
        // 回転 (右ボタンドラッグ)
        if (input.is_right_mouse_down) {
            new_camera.yaw -= input.delta_yaw * mouse_sensitivity;
            new_camera.pitch += input.delta_pitch * mouse_sensitivity;
            
            // ピッチの制限
            if (new_camera.pitch > 89.0f)  new_camera.pitch = 89.0f;
            if (new_camera.pitch < -89.0f) new_camera.pitch = -89.0f;
        }

        // パン (中ボタンドラッグ or Shift+右ドラッグ)
        if (input.is_middle_mouse_down || (input.is_right_mouse_down && (input.move_y != 0.0f))) {
            glm::vec3 front;
            front.x = std::cos(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
            front.y = std::sin(glm::radians(new_camera.pitch));
            front.z = std::sin(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
            front = glm::normalize(front);
            
            glm::vec3 right = glm::normalize(glm::cross(front, new_camera.up));
            glm::vec3 up = glm::normalize(glm::cross(right, front));

            new_camera.target -= right * input.delta_yaw * pan_speed;
            new_camera.target += up * input.delta_pitch * pan_speed;
        }

        // ズーム (ホイール)
        if (input.scroll_y != 0.0f) {
            new_camera.distance -= input.scroll_y * zoom_speed;
            if (new_camera.distance < 0.1f) new_camera.distance = 0.1f; // 最小距離
        }
        
        // Target と distance から position を逆算
        glm::vec3 front;
        front.x = std::cos(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
        front.y = std::sin(glm::radians(new_camera.pitch));
        front.z = std::sin(glm::radians(new_camera.yaw)) * std::cos(glm::radians(new_camera.pitch));
        front = glm::normalize(front);
        
        new_camera.position = new_camera.target - front * new_camera.distance;

    } else { // Fly mode
        if (input.is_right_mouse_down) {
            new_camera.yaw += input.delta_yaw * mouse_sensitivity;
            new_camera.pitch -= input.delta_pitch * mouse_sensitivity; // Invert pitch for fly

            if (new_camera.pitch > 89.0f)  new_camera.pitch = 89.0f;
            if (new_camera.pitch < -89.0f) new_camera.pitch = -89.0f;
        }

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
    }

    return new_camera;
}

[[nodiscard]] glm::mat4 compute_view_matrix(const CameraData& camera) noexcept {
    if (camera.mode == CameraMode::Orbit) {
        return glm::lookAt(camera.position, camera.target, camera.up);
    } else {
        glm::vec3 front;
        front.x = std::cos(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch));
        front.y = std::sin(glm::radians(camera.pitch));
        front.z = std::sin(glm::radians(camera.yaw)) * std::cos(glm::radians(camera.pitch));
        front = glm::normalize(front);
        return glm::lookAt(camera.position, camera.position + front, camera.up);
    }
}

[[nodiscard]] glm::mat4 compute_projection_matrix(const CameraData& camera) noexcept {
    auto proj = glm::perspective(glm::radians(camera.fov_degrees), camera.aspect_ratio, camera.near_plane, camera.far_plane);
    proj[1][1] *= -1.0f; // Vulkan Y-flip
    return proj;
}

[[nodiscard]] InputState poll_input(GLFWwindow* window, MouseTracker& tracker, float scroll_y) noexcept {
    InputState state{};
    state.scroll_y = scroll_y;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.move_z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.move_z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.move_x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.move_x += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) state.move_y += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) state.move_y -= 1.0f;

    state.is_right_mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    state.is_middle_mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    state.is_left_mouse_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    double current_x, current_y;
    glfwGetCursorPos(window, &current_x, &current_y);

    if (tracker.first_mouse) {
        tracker.last_x = current_x;
        tracker.last_y = current_y;
        tracker.first_mouse = false;
    }

    state.delta_yaw = static_cast<float>(current_x - tracker.last_x);
    state.delta_pitch = static_cast<float>(tracker.last_y - current_y);

    tracker.last_x = current_x;
    tracker.last_y = current_y;

    return state;
}

}  // namespace vanta::scene
