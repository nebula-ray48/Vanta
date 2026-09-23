//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "vulkan_buffer_utils.h"

#include <cstring>

std::expected<AllocatedBuffer, std::string> upload_buffer_to_gpu(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandPool command_pool,
    VkQueue graphics_queue,
    size_t buffer_size,
    const void* data,
    VkBufferUsageFlags target_usage)
{

    VkBufferCreateInfo staging_info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    staging_info.size = buffer_size;
    staging_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo staging_alloc_info = {};
    staging_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    staging_alloc_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    AllocatedBuffer staging_buffer;
    VmaAllocationInfo alloc_info;
    if (vmaCreateBuffer(allocator, &staging_info, &staging_alloc_info,
                        &staging_buffer.buffer, &staging_buffer.allocation, &alloc_info) != VK_SUCCESS) {
        return std::unexpected("Failed to create staging buffer");
    }

    std::memcpy(alloc_info.pMappedData, data, buffer_size);

    VkBufferCreateInfo target_info = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    target_info.size = buffer_size;
    target_info.usage = target_usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo target_alloc_info = {};
    target_alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    AllocatedBuffer target_buffer;
    if (vmaCreateBuffer(allocator, &target_info, &target_alloc_info,
                        &target_buffer.buffer, &target_buffer.allocation, nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);
        return std::unexpected("Failed to create target GPU buffer");
    }

    VkCommandBufferAllocateInfo cmd_alloc_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandPool = command_pool;
    cmd_alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(device, &cmd_alloc_info, &cmd) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, target_buffer.buffer, target_buffer.allocation);
        vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);
        return std::unexpected("Failed to allocate command buffer for transfer");
    }

    VkCommandBufferBeginInfo begin_info = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkBufferCopy copy_region = { .size = buffer_size };
    vkCmdCopyBuffer(cmd, staging_buffer.buffer, target_buffer.buffer, 1, &copy_region);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &cmd);
    vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);

    return target_buffer;
}

