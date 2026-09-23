//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "vulkan_context.h"

#include <string_view>
#include <vector>

#define VMA_IMPLEMENTATION
#include "../../include/ext/glfw3.h"

namespace vanta::render {

[[nodiscard]] std::expected<VulkanContext, EngineError> create_vulkan_context(
    const char* app_name,
    void* window_handle
) {
    VulkanContext ctx{};

    // ============================================================================
    // 1. Instance の作成
    // ============================================================================
    VkApplicationInfo const app_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = app_name,
        .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .pEngineName = "Rey Engine",
        .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4
    };

    // GLFWが現在のOS(Win/Mac/Linux)で必要とするSurface拡張機能を自動で取得する
    uint32_t glfw_ext_count = 0;
    const char** glfw_exts = glfwGetRequiredInstanceExtensions(&glfw_ext_count);
    if (!glfw_exts) {
        return std::unexpected(LegacyError("GLFW: 必要なVulkan拡張機能の取得に失敗しました"));
    }
    std::vector<const char*> instance_extensions(glfw_exts, glfw_exts + glfw_ext_count);

    // Mac (MoltenVK) 特有の拡張機能を、Macビルド時のみ追加する
    VkInstanceCreateFlags instance_flags = 0;
#ifdef __APPLE__
    instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
    instance_flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    std::vector<const char*> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
    };

    VkPhysicalDeviceVulkan12Features vk12_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = nullptr,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingUpdateUnusedWhilePending = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE
    };

    VkPhysicalDeviceVulkan13Features vk13_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &vk12_features,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
    };

    VkInstanceCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = instance_flags,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = static_cast<uint32_t>(validation_layers.size()),
        .ppEnabledLayerNames = validation_layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size()),
        .ppEnabledExtensionNames = instance_extensions.data()
    };

    if (vkCreateInstance(&create_info, nullptr, &ctx.instance) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Vulkan: vkCreateInstance の呼び出しに失敗しました"));
    }

    if (glfwCreateWindowSurface(ctx.instance, static_cast<GLFWwindow*>(window_handle), nullptr, &ctx.surface) != VK_SUCCESS) {
        ctx.destroy();
        return std::unexpected(LegacyError("GLFW: Window Surface の生成に失敗しました"));
    }

    // ============================================================================
    // 2. Physical Device の選択
    // ============================================================================
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, nullptr);
    if (device_count == 0) {
        ctx.destroy();
        return std::unexpected(LegacyError("Vulkan: Vulkan対応のGPUが見つかりません"));
    }

    std::vector<VkPhysicalDevice> physical_devices(device_count);
    vkEnumeratePhysicalDevices(ctx.instance, &device_count, physical_devices.data());

    bool device_found = false;
    for (const auto& p_device : physical_devices) {
        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, nullptr);
        std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, queue_families.data());

        for (uint32_t i = 0; i < queue_family_count; i++) {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 present_support = false;
                if (ctx.surface != VK_NULL_HANDLE) {
                    vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, ctx.surface, &present_support);
                }

                if (ctx.surface == VK_NULL_HANDLE || present_support) {
                    ctx.physical_device = p_device;
                    ctx.graphics_queue_family_index = i;
                    device_found = true;
                    break;
                }
            }
        }
        if (device_found) break;
    }

    if (!device_found) {
        ctx.destroy();
        return std::unexpected(LegacyError("Vulkan: 描画と表示に対応したGPUキューが見つかりません"));
    }

    // ============================================================================
    // 3. Logical Device と Dynamic Rendering の有効化
    // ============================================================================
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo const queue_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = ctx.graphics_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };

    std::vector<const char*> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(ctx.physical_device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(ctx.physical_device, nullptr, &extension_count, available_extensions.data());

    for (const auto& ext : available_extensions) {
        std::string_view const ext_name(ext.extensionName);
        // MoltenVK 等のポータビリティサブセット環境への対応
        if (ext_name == "VK_KHR_portability_subset") {
            device_extensions.push_back("VK_KHR_portability_subset");
        }
        if (ext_name == VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) {
            device_extensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
        }
    }

    constexpr VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo const device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vk13_features,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = static_cast<uint32_t>(device_extensions.size()),
        .ppEnabledExtensionNames = device_extensions.data(),
        .pEnabledFeatures = &device_features
    };

    if (vkCreateDevice(ctx.physical_device, &device_create_info, nullptr, &ctx.device) != VK_SUCCESS) {
        ctx.destroy();
        return std::unexpected(LegacyError("Vulkan: 論理デバイス(VkDevice)の生成に失敗しました"));
    }

    vkGetDeviceQueue(ctx.device, ctx.graphics_queue_family_index, 0, &ctx.graphics_queue);

    // ============================================================================
    // 4. VMAの初期化
    // ============================================================================
    VmaAllocatorCreateInfo const allocator_info{
        .flags = 0,
        .physicalDevice = ctx.physical_device,
        .device = ctx.device,
        .instance = ctx.instance,
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };

    if (vmaCreateAllocator(&allocator_info, &ctx.allocator) != VK_SUCCESS) {
        ctx.destroy();
        return std::unexpected(LegacyError("VMA: アロケータの初期化に失敗しました"));
    }

    return ctx;
}

}  // namespace vanta::render

