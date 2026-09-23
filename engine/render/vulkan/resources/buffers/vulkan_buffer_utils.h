//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <string>

#include "ext/vk_mem_alloc.h"

// バッファとそのメモリ割り当て情報をセットで管理する構造体
struct AllocatedBuffer {
    VkBuffer buffer{VK_NULL_HANDLE};
    VmaAllocation allocation{nullptr};
};

std::expected<AllocatedBuffer, std::string> upload_buffer_to_gpu(
    VmaAllocator allocator,
    VkDevice device,
    VkCommandPool command_pool,
    VkQueue graphics_queue,
    size_t buffer_size,
    const void* data,
    VkBufferUsageFlags target_usage
);

