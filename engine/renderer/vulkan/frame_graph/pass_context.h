#pragma once

#include <vulkan/vulkan.h>
#include "vulkan/frame_graph/render_graph_types.h"
#include "vulkan/resources/resource_registry.h"

#include <unordered_map>

namespace vanta::render::fg {

class PassContext {
public:
    PassContext(VkCommandBuffer cmd, const ResourceRegistry& registry, const std::unordered_map<uint64_t, ImageHandle>& handle_map) noexcept
        : cmd_(cmd), registry_(registry), handle_map_(handle_map) {}

    [[nodiscard]] VkCommandBuffer command_buffer() const noexcept { return cmd_; }

    [[nodiscard]] VkImage get_image(ImageHandle handle) const noexcept {
        uint64_t key = (static_cast<uint64_t>(handle.index) << 32) | handle.generation;
        if (auto it = handle_map_.find(key); it != handle_map_.end()) {
            return registry_.get_vk_image(it->second);
        }
        return registry_.get_vk_image(handle);
    }

    [[nodiscard]] VkImageView get_image_view(ImageHandle handle) const noexcept {
        uint64_t key = (static_cast<uint64_t>(handle.index) << 32) | handle.generation;
        if (auto it = handle_map_.find(key); it != handle_map_.end()) {
            return registry_.get_vk_image_view(it->second);
        }
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
    const std::unordered_map<uint64_t, ImageHandle>& handle_map_;
};

} // namespace vanta::render::fg
