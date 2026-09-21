#pragma once
#include <vulkan/vulkan.h>

namespace vanta::render {

class VulkanDevice {
public:
    VulkanDevice();
    ~VulkanDevice();

    // コピーと移動を禁止（単一のデバイスインスタンスを保証）
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    [[nodiscard]] VkDevice device() const noexcept { return device_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const noexcept { return physical_device_; }

    [[nodiscard]] VkQueue graphics_queue() const noexcept { return graphics_queue_; }
    [[nodiscard]] VkQueue compute_queue() const noexcept { return compute_queue_; }
    [[nodiscard]] VkQueue transfer_queue() const noexcept { return transfer_queue_; }

    [[nodiscard]] bool supports_descriptor_indexing() const noexcept { return supports_descriptor_indexing_; }
    [[nodiscard]] bool supports_synchronization2() const noexcept { return supports_synchronization2_; }

private:
    VkInstance instance_{VK_NULL_HANDLE};
    VkPhysicalDevice physical_device_{VK_NULL_HANDLE};
    VkDevice device_{VK_NULL_HANDLE};

    VkQueue graphics_queue_{VK_NULL_HANDLE};
    VkQueue compute_queue_{VK_NULL_HANDLE};
    VkQueue transfer_queue_{VK_NULL_HANDLE};

    bool supports_descriptor_indexing_{false};
    bool supports_synchronization2_{false};

    // 内部の初期化処理
    void create_instance();
    void pick_physical_device();
    void create_logical_device();
};

}  // namespace vanta::render
