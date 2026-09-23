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
        return registry_.get_vk_image(to_registry_handle(handle));
    }

    [[nodiscard]] VkImageView get_image_view(ImageHandle handle) const noexcept {
        return registry_.get_vk_image_view(to_registry_handle(handle));
    }

    [[nodiscard]] VkBuffer get_buffer(BufferHandle handle) const noexcept {
        return registry_.get_vk_buffer(to_registry_handle(handle));
    }

    [[nodiscard]] const ResourceRegistry& resources() const noexcept {
        return registry_;
    }

private:
    static ::vanta::render::ImageHandle to_registry_handle(ImageHandle handle) noexcept {
        return {handle.id, handle.generation};
    }
    static ::vanta::render::BufferHandle to_registry_handle(BufferHandle handle) noexcept {
        return {handle.id, handle.generation};
    }
    VkCommandBuffer cmd_;
    const ResourceRegistry& registry_;
};

} // namespace vanta::render::fg
