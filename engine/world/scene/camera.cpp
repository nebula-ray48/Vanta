#include <GLFW/glfw3.h>
#include "world/scene/camera.h"

namespace vanta::scene {

[[nodiscard]] scene::InputState poll_input(GLFWwindow* window, MouseTracker& tracker) noexcept {
    scene::InputState state{};

    // キーボード入力（W, A, S, D, Q, E）の集計
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) state.move_z += 1.0f; // 前進
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) state.move_z -= 1.0f; // 後退
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) state.move_x -= 1.0f; // 左
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) state.move_x += 1.0f; // 右
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) state.move_y += 1.0f; // 上昇
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) state.move_y -= 1.0f; // 下降

    // マウス入力の集計
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
