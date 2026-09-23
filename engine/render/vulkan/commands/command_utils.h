//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>

#include "../core/vulkan_context.h"

#include "engine_error.h"

namespace vanta::render {

    [[nodiscard]] std::expected<void, EngineError> copy_buffer(
        const VulkanContext& context,
        VkCommandPool command_pool,
        VkBuffer src_buffer,
        VkBuffer dst_buffer,
        VkDeviceSize size);

} // namespace vanta::render
