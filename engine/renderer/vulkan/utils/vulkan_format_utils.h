#pragma once

#include <vulkan/vulkan.h>

namespace vanta::render {

[[nodiscard]] constexpr VkImageAspectFlags get_image_aspect_mask(VkFormat format) noexcept {
    switch (format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case VK_FORMAT_S8_UINT:
            return VK_IMAGE_ASPECT_STENCIL_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

[[nodiscard]] constexpr bool is_depth_format(VkFormat format) noexcept {
    return (get_image_aspect_mask(format) & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
}

[[nodiscard]] constexpr bool is_stencil_format(VkFormat format) noexcept {
    return (get_image_aspect_mask(format) & VK_IMAGE_ASPECT_STENCIL_BIT) != 0;
}

} // namespace vanta::render
