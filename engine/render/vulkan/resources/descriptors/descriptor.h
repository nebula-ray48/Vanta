//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <expected>

#include "engine_error.h"
#include "vulkan/core/vulkan_context.h"

namespace vanta::render {

    struct alignas(16) GlobalUbo {
        glm::mat4 view_proj;
        glm::vec3 camera_pos;
        float padding;
    };

    class BindlessDescriptorLayout {
    public:
        [[nodiscard]] static std::expected<VkDescriptorSetLayout, EngineError> create(VkDevice device) noexcept;

        static void destroy(VkDevice device, VkDescriptorSetLayout layout) noexcept;
    };

    class BindlessDescriptorManager {
    public:

        [[nodiscard]] static std::expected<VkDescriptorPool, EngineError> create_pool(VkDevice device) noexcept;

        // Poolの破棄
        static void destroy_pool(VkDevice device, VkDescriptorPool pool) noexcept;

        [[nodiscard]] static std::expected<VkDescriptorSet, EngineError> allocate_set(
            VkDevice device,
            VkDescriptorPool pool,
            VkDescriptorSetLayout layout) noexcept;

        static void update_ubo(
            VkDevice device,
            VkDescriptorSet set,
            VkBuffer ubo_buffer,
            size_t ubo_size) noexcept;
    };

}  // namespace vanta::render

