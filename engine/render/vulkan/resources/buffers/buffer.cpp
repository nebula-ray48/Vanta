//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//



#include "engine_error.h"
#include "ext/vk_mem_alloc.h"
#include "vulkan/core/vulkan_context.h"

namespace vanta::render {
    struct VulkanContext;
    struct GpuBuffer;

    std::expected<GpuBuffer, EngineError> create_buffer(
        const VulkanContext& context,
        const VkDeviceSize size,
        const VkBufferUsageFlags usage,
        const VmaAllocationCreateInfo& alloc_create_info)
    {
        const VkBufferCreateInfo buffer_info{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };

        VkBuffer buffer{};
        VmaAllocation allocation{};
        if (vmaCreateBuffer(context.allocator, &buffer_info, &alloc_create_info,
                             &buffer, &allocation, nullptr) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"バッファ作成失敗"}});
        }

        return GpuBuffer{
            .buffer = buffer,
            .allocation = allocation,
            .size = size,
        };
    }

    std::expected<void, EngineError> copy_buffer(
        const VulkanContext& context,
        VkCommandPool command_pool,
        VkBuffer src,
        VkBuffer dst,
        const VkDeviceSize size)
    {
        const VkCommandBufferAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        VkCommandBuffer cmd{};
        if (vkAllocateCommandBuffers(context.device, &alloc_info, &cmd) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"コピー用コマンドバッファの確保失敗"}});
        }

        constexpr VkCommandBufferBeginInfo begin_info{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        vkBeginCommandBuffer(cmd, &begin_info);

        const VkBufferCopy copy_region{.srcOffset = 0, .dstOffset = 0, .size = size};
        vkCmdCopyBuffer(cmd, src, dst, 1, &copy_region);

        vkEndCommandBuffer(cmd);

        const VkSubmitInfo submit_info{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
        };

        if (vkQueueSubmit(context.graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
            vkFreeCommandBuffers(context.device, command_pool, 1, &cmd);
            return std::unexpected(EngineError{LegacyError{"コピーコマンドのSubmit失敗"}});
        }
        vkQueueWaitIdle(context.graphics_queue);

        vkFreeCommandBuffers(context.device, command_pool, 1, &cmd);
        return {};
    }

}  // namespace vanta::render
