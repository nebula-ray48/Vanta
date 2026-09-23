#pragma once

#include <vulkan/vulkan.h>
#include "vulkan/frame_graph/render_graph_types.h"
#include "vulkan/resources/resource_registry.h"

namespace vanta::render::fg {

class PassContext {
public:
    PassContext(VkCommandBuffer cmd, const ResourceRegistry& registry) noexcept
        : cmd_(cmd), registry_(registry) {}

    [[nodiscard]] VkCommandBuffer command_buffer() const noexcept { return cmd_; }

    [[nodiscard]] VkImage get_image(ImageHandle handle) const noexcept {
        return registry_.get_vk_image(handle);
    }

    [[nodiscard]] VkImageView get_image_view(ImageHandle handle) const noexcept {
        return registry_.get_vk_image_view(handle);
    }

    [[nodiscard]] VkBuffer get_buffer(BufferHandle handle) const noexcept {
        return registry_.get_vk_buffer(handle);
    }

    [[nodiscard]] const ResourceRegistry& resources() const noexcept {
        return registry_;
    }

private:
    VkCommandBuffer cmd_;
    const ResourceRegistry& registry_;
};

} // namespace vanta::render::fg
