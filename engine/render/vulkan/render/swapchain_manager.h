#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace vanta::render {

class VulkanDevice;

class SwapchainManager {
public:
    // 作成には VulkanDevice と ウィンドウの Surface が必要
    SwapchainManager(const VulkanDevice& device, VkSurfaceKHR surface);
    ~SwapchainManager();

    // ウィンドウサイズ変更時や OUT_OF_DATE 時に呼ばれる
    bool recreate(uint32_t width, uint32_t height);

    // 次の画像を取得（begin_frame の役割）
    VkResult acquire_next_image(VkSemaphore present_complete, uint32_t& image_index);

    // 画面に表示（end_frame の役割）
    VkResult present(VkQueue queue, VkSemaphore render_complete, uint32_t image_index);

    [[nodiscard]] VkFormat image_format() const noexcept { return image_format_; }
    [[nodiscard]] VkExtent2D extent() const noexcept { return extent_; }

private:
    const VulkanDevice& device_;
    VkSurfaceKHR surface_{VK_NULL_HANDLE};
    VkSwapchainKHR swapchain_{VK_NULL_HANDLE};

    std::vector<VkImage> images_;
    std::vector<VkImageView> image_views_;

    VkFormat image_format_;
    VkExtent2D extent_;

    void cleanup();
};

}
