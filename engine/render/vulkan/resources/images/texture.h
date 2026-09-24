//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once
#include <vulkan/vulkan.h>

#include <expected>

#include "assets/image_loader.hpp"

namespace vanta::vulkan {

    enum class VulkanError {
        ALLOCATION_FAILED,
        TRANSFER_FAILED
    };

    class Texture {
    public:
        // ムーブセマンティクスのみ許可（Always-Valid）
        Texture(Texture&& other) noexcept;
        Texture& operator=(Texture&& other) noexcept;
        ~Texture();

        // コピー禁止
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        [[nodiscard]] VkImageView get_view() const noexcept { return image_view; }
        [[nodiscard]] VkSampler get_sampler() const noexcept { return sampler; }

    private:
        // ファクトリ関数のみがインスタンスを生成可能
        friend std::expected<Texture, VulkanError> create_texture_from_image(
            VkDevice device,
            VkPhysicalDevice physical_device,
            VkCommandPool command_pool,
            VkQueue graphics_queue,
            const RawImage& image,
            VkFormat format
        );

        friend std::expected<Texture, VulkanError> create_cubemap_from_hdr_mips(
            VkDevice device,
            VkPhysicalDevice physical_device,
            VkCommandPool command_pool,
            VkQueue graphics_queue,
            const std::filesystem::path& base_dir,
            uint32_t mip_count
        );

        Texture() = default;

        VkDevice device = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView image_view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    // 外部から叩くファクトリ関数
    [[nodiscard]] std::expected<Texture, VulkanError> create_texture_from_image(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        const RawImage& image,
        VkFormat format = VK_FORMAT_R8G8B8A8_SRGB
    );

    [[nodiscard]] std::expected<Texture, VulkanError> create_cubemap_from_hdr_mips(
        VkDevice device,
        VkPhysicalDevice physical_device,
        VkCommandPool command_pool,
        VkQueue graphics_queue,
        const std::filesystem::path& base_dir,
        uint32_t mip_count = 6
    );

    class BindlessManager {
    public:
        static void write_texture(VkDevice device, VkDescriptorSet set, uint32_t binding, uint32_t index, const Texture& texture);
    };
}  // namespace vanta::vulkan

