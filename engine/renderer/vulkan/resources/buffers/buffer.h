//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <span>
#include <type_traits>

#include "include/ext/vk_mem_alloc.h"
#include "vulkan/core/vulkan_context.h"
#include "include/engine_error.h"

namespace vanta::render {
    struct GpuBuffer;
    struct VulkanContext;

    template <typename T>
    concept GpuPodLike = std::is_trivially_copyable_v<T> && std::is_standard_layout_v<T>;

    [[nodiscard]] std::expected<GpuBuffer, EngineError> create_buffer(
        const VulkanContext& context,
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        const VmaAllocationCreateInfo& alloc_create_info);

    [[nodiscard]] std::expected<void, EngineError> copy_buffer(
        const VulkanContext& context,
        VkCommandPool command_pool,
        VkBuffer src,
        VkBuffer dst,
        VkDeviceSize size);

    template <GpuPodLike T>
    [[nodiscard]] std::expected<GpuBuffer, EngineError> create_device_local_buffer(
        const VulkanContext& context,
        VkCommandPool command_pool,
        std::span<const T> data,
        VkBufferUsageFlags usage_flags)
    {
        const VkDeviceSize size = static_cast<VkDeviceSize>(sizeof(T) * data.size());
        if (size == 0) { return std::unexpected(EngineError{LegacyError{"データサイズ0"}}); }

        constexpr VmaAllocationCreateInfo staging_alloc_info{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO,
        };
        auto staging = create_buffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging_alloc_info);
        if (!staging) { return std::unexpected(staging.error()); }

        if (vmaCopyMemoryToAllocation(context.allocator, data.data(), staging->allocation, 0, size) != VK_SUCCESS) {
            staging->destroy(context);
            return std::unexpected(EngineError{LegacyError{"Staging書き込み失敗"}});
        }

        constexpr VmaAllocationCreateInfo device_alloc_info{
            .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        };
        auto device = create_buffer(context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage_flags, device_alloc_info);
        if (!device) {
            staging->destroy(context);
            return std::unexpected(device.error());
        }

        if (auto res = copy_buffer(context, command_pool, staging->buffer, device->buffer, size); !res) {
            staging->destroy(context);
            device->destroy(context);
            return std::unexpected(res.error());
        }

        staging->destroy(context);
        return *device;
    }

    template <GpuPodLike T>
    [[nodiscard]] std::expected<void, EngineError> update_buffer_data(
        const VulkanContext& context,
        const GpuBuffer& gpu_buffer,
        const T& data)
    {
        const VkDeviceSize data_size = static_cast<VkDeviceSize>(sizeof(T));
        if (data_size > gpu_buffer.size) {
            return std::unexpected(EngineError{LegacyError{"バッファサイズ超過"}});
        }

        if (vmaCopyMemoryToAllocation(context.allocator, &data, gpu_buffer.allocation, 0, data_size) != VK_SUCCESS) {
            return std::unexpected(EngineError{LegacyError{"バッファ更新失敗"}});
        }
        return {};
    }

}  // namespace vanta::render
