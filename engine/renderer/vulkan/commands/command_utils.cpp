//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "command_utils.h"

namespace vanta::render {

    std::expected<void, EngineError> copy_buffer(
        const VulkanContext& context,
        VkCommandPool command_pool,
        VkBuffer src_buffer,
        VkBuffer dst_buffer,
        VkDeviceSize size)
    {
        const VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VkCommandBuffer command_buffer{VK_NULL_HANDLE};
        if (vkAllocateCommandBuffers(context.device, &alloc_info, &command_buffer) != VK_SUCCESS) {
            return std::unexpected(LegacyError("コマンドバッファの割り当てに失敗"));
        }

        constexpr VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device, command_pool, 1, &command_buffer);
            return std::unexpected(LegacyError("コマンドバッファの開始に失敗"));
        }

        const VkBufferCopy copy_region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = size,
        };

        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

        if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device, command_pool, 1, &command_buffer);
            return std::unexpected(LegacyError("コマンドバッファの終了に失敗"));
        }

        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &command_buffer,
        };

        if (vkQueueSubmit(context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device, command_pool, 1, &command_buffer);
            return std::unexpected(LegacyError("キューの送信に失敗"));
        }

        if (vkQueueWaitIdle(context.graphics_queue) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device, command_pool, 1, &command_buffer);
            return std::unexpected(LegacyError("キューの待機中にエラーが発生"));
        }

        vkFreeCommandBuffers(context.device, command_pool, 1, &command_buffer);
        return {};
    }

}  // namespace vanta::render

