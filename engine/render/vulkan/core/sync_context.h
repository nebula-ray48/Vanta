//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <vector>

#include "engine_error.h"

namespace vanta::render {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct SyncContext {
    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<VkFence> in_flight_fences;
    VkCommandPool command_pool{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> command_buffers;
    size_t current_frame{0};

    [[nodiscard]] static std::expected<SyncContext, EngineError> create(
        VkDevice device,
        uint32_t queue_family_index,
        uint32_t image_count
    ) noexcept {
        SyncContext ctx;

        VkSemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初期状態でシグナル済み

        // メモリの再確保防止 (Rustの with_capacity に相当)
        ctx.image_available_semaphores.reserve(MAX_FRAMES_IN_FLIGHT);
        ctx.in_flight_fences.reserve(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VkSemaphore sem;
            if (vkCreateSemaphore(device, &semaphore_info, nullptr, &sem) != VK_SUCCESS) {
                return std::unexpected(LegacyError("image_available セマフォの作成に失敗"));
            }
            ctx.image_available_semaphores.push_back(sem);

            VkFence fence;
            if (vkCreateFence(device, &fence_info, nullptr, &fence) != VK_SUCCESS) {
                return std::unexpected(LegacyError("in_flight フェンスの作成に失敗"));
            }
            ctx.in_flight_fences.push_back(fence);
        }

        ctx.render_finished_semaphores.reserve(image_count);
        for (uint32_t i = 0; i < image_count; ++i) {
            VkSemaphore sem;
            if (vkCreateSemaphore(device, &semaphore_info, nullptr, &sem) != VK_SUCCESS) {
                return std::unexpected(LegacyError("render_finished セマフォの作成に失敗"));
            }
            ctx.render_finished_semaphores.push_back(sem);
        }

        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = queue_family_index;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device, &pool_info, nullptr, &ctx.command_pool) != VK_SUCCESS) {
            return std::unexpected(LegacyError("コマンドプールの作成に失敗"));
        }

        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = ctx.command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        ctx.command_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateCommandBuffers(device, &alloc_info, ctx.command_buffers.data()) != VK_SUCCESS) {
            return std::unexpected(LegacyError("コマンドバッファの割り当てに失敗"));
        }
        return ctx;
    }

    void destroy(VkDevice device) const noexcept {
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
        }

        for (auto sem : image_available_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto sem : render_finished_semaphores) {
            vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto fence : in_flight_fences) {
            vkDestroyFence(device, fence, nullptr);
        }
    }
};

}  // namespace vanta::render
