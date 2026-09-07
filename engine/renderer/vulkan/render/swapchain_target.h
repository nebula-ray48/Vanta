//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <ranges>
#include <vector>

#include "../core/vulkan_context.h"

#include "engine_error.h"

namespace vanta::render {

    struct SwapchainTarget {
        VkSwapchainKHR swapchain{VK_NULL_HANDLE};
        VkFormat format{VK_FORMAT_UNDEFINED};
        VkExtent2D extent{.width = 0, .height = 0};

        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
        std::vector<VkFramebuffer> framebuffers;

        VkFormat depth_format{VK_FORMAT_UNDEFINED};
        VkImage depth_image{VK_NULL_HANDLE};
        VkDeviceMemory depth_image_memory{VK_NULL_HANDLE};
        VkImageView depth_image_view{VK_NULL_HANDLE};

        VkRenderPass render_pass{VK_NULL_HANDLE};


        [[nodiscard]] auto image_resources() const noexcept {
            return std::views::zip(images, image_views, framebuffers);
        }

        void destroy(VkDevice device) const noexcept;
    };

    [[nodiscard]] std::expected<SwapchainTarget, EngineError> create_swapchain_target(
        const VulkanContext& context,
        uint32_t width,
        uint32_t height
    );

}  // namespace vanta::render

