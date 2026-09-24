//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "texture.h"

#include <stdexcept>
#include <cstring>
#include <vector>
#include <string>
#include <iostream>
#include <glm/gtc/packing.hpp>

namespace vanta::vulkan {

Texture::Texture(Texture&& other) noexcept
    : device(other.device), image(other.image), memory(other.memory),
      image_view(other.image_view), sampler(other.sampler) {
    other.device = VK_NULL_HANDLE;
    other.image = VK_NULL_HANDLE;
    other.memory = VK_NULL_HANDLE;
    other.image_view = VK_NULL_HANDLE;
    other.sampler = VK_NULL_HANDLE;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        this->~Texture();
        device = other.device;
        image = other.image;
        memory = other.memory;
        image_view = other.image_view;
        sampler = other.sampler;

        other.device = VK_NULL_HANDLE;
        other.image = VK_NULL_HANDLE;
        other.memory = VK_NULL_HANDLE;
        other.image_view = VK_NULL_HANDLE;
        other.sampler = VK_NULL_HANDLE;
    }
    return *this;
}

Texture::~Texture() {
    if (device != VK_NULL_HANDLE) {
        if (sampler != VK_NULL_HANDLE) vkDestroySampler(device, sampler, nullptr);
        if (image_view != VK_NULL_HANDLE) vkDestroyImageView(device, image_view, nullptr);
        if (image != VK_NULL_HANDLE) vkDestroyImage(device, image, nullptr);
        if (memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
    }
}


uint32_t find_memory_type(VkPhysicalDevice physical_device, uint32_t type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

std::expected<Texture, VulkanError> create_texture_from_image(
    VkDevice device, VkPhysicalDevice physical_device,
    VkCommandPool command_pool, VkQueue graphics_queue, const RawImage& image, VkFormat format) {

    if (!image.data || image.width == 0 || image.height == 0) return std::unexpected(VulkanError::TRANSFER_FAILED);

    VkDeviceSize image_size = static_cast<VkDeviceSize>(image.width) * static_cast<VkDeviceSize>(image.height) * 4;
    Texture tex;
    tex.device = device;

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = image_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, staging_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &alloc_info, nullptr, &staging_buffer_memory) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);
    vkBindBufferMemory(device, staging_buffer, staging_buffer_memory, 0);

    // 2. データのマッピングとコピー
    void* data;
    vkMapMemory(device, staging_buffer_memory, 0, image_size, 0, &data);
    memcpy(data, image.data.get(), static_cast<size_t>(image_size));
    vkUnmapMemory(device, staging_buffer_memory);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = static_cast<uint32_t>(image.width);
    image_info.extent.height = static_cast<uint32_t>(image.height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &image_info, nullptr, &tex.image) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);

    vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);
    vkBindImageMemory(device, tex.image, tex.memory, 0);

    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandPool = command_pool;
    cmd_alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmd_alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    // Layout Transition: Undefined -> Transfer Dst
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Buffer to Image Copy
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1};
    vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Layout Transition: Transfer Dst -> Shader Read Only
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);
    vkFreeCommandBuffers(device, command_pool, 1, &cmd);

    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_buffer_memory, nullptr);

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = tex.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; // ここでUVのタイリングを有効化
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    if (vkCreateSampler(device, &sampler_info, nullptr, &tex.sampler) != VK_SUCCESS) return std::unexpected(VulkanError::ALLOCATION_FAILED);

    return tex;
}

// 外部のstbi_loadf宣言
extern "C" float* stbi_loadf(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);

std::expected<Texture, VulkanError> create_cubemap_from_hdr_mips(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkCommandPool command_pool,
    VkQueue graphics_queue,
    const std::filesystem::path& base_dir,
    uint32_t mip_count)
{
    const std::array<std::string, 6> face_names = { "px", "nx", "py", "ny", "pz", "nz" };

    struct MipFaceData {
        int width = 0;
        int height = 0;
        float* pixels = nullptr;
        VkDeviceSize buffer_offset = 0;
        VkDeviceSize size_bytes = 0;
    };

    std::vector<std::array<MipFaceData, 6>> all_mips(mip_count);
    VkDeviceSize total_staging_size = 0;

    for (uint32_t m = 0; m < mip_count; ++m) {
        for (size_t f = 0; f < 6; ++f) {
            std::string filename = "m" + std::to_string(m) + "_" + face_names[f] + ".hdr";
            std::filesystem::path file_path = base_dir / filename;

            int w = 0, h = 0, channels = 0;
            float* data = stbi_loadf(file_path.string().c_str(), &w, &h, &channels, 4);
            if (!data) {
                std::cerr << "[IBL ERROR] Failed to load HDR file: " << file_path << "\n";
                // Free already loaded
                for (uint32_t pm = 0; pm <= m; ++pm) {
                    for (size_t pf = 0; pf < 6; ++pf) {
                        if (all_mips[pm][pf].pixels) stbi_image_free(all_mips[pm][pf].pixels);
                    }
                }
                return std::unexpected(VulkanError::TRANSFER_FAILED);
            }

            if (m == 0 && f == 0) {
                std::cout << "[IBL DEBUG] Loaded " << file_path << " (" << w << "x" << h << ") channels: " << channels
                          << " pixel[0]: " << data[0] << ", " << data[1] << ", " << data[2] << ", " << data[3] << "\n";
            }

            VkDeviceSize size = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * 4 * sizeof(uint16_t);
            all_mips[m][f] = MipFaceData{
                .width = w,
                .height = h,
                .pixels = data,
                .buffer_offset = total_staging_size,
                .size_bytes = size,
            };
            total_staging_size += size;
        }
    }

    uint32_t base_width = static_cast<uint32_t>(all_mips[0][0].width);
    uint32_t base_height = static_cast<uint32_t>(all_mips[0][0].height);

    // Staging buffer creation
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = total_staging_size;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &buffer_info, nullptr, &staging_buffer) != VK_SUCCESS) {
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }

    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(device, staging_buffer, &mem_reqs);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &alloc_info, nullptr, &staging_buffer_memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, staging_buffer, nullptr);
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }
    vkBindBufferMemory(device, staging_buffer, staging_buffer_memory, 0);

    // Copy to staging with float -> half (FP16) conversion
    void* mapped_ptr = nullptr;
    vkMapMemory(device, staging_buffer_memory, 0, total_staging_size, 0, &mapped_ptr);
    for (uint32_t m = 0; m < mip_count; ++m) {
        for (size_t f = 0; f < 6; ++f) {
            const auto& face = all_mips[m][f];
            uint16_t* dst_ptr = reinterpret_cast<uint16_t*>(static_cast<char*>(mapped_ptr) + face.buffer_offset);
            size_t num_components = static_cast<size_t>(face.width) * static_cast<size_t>(face.height) * 4;
            for (size_t i = 0; i < num_components; ++i) {
                dst_ptr[i] = glm::packHalf1x16(face.pixels[i]);
            }
            stbi_image_free(face.pixels);
        }
    }
    vkUnmapMemory(device, staging_buffer_memory);

    // Create Cubemap Image (VK_FORMAT_R16G16B16A16_SFLOAT allows linear filtering on Metal/Apple GPUs)
    Texture tex;
    tex.device = device;

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = base_width;
    image_info.extent.height = base_height;
    image_info.extent.depth = 1;
    image_info.mipLevels = mip_count;
    image_info.arrayLayers = 6;
    image_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &image_info, nullptr, &tex.image) != VK_SUCCESS) {
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }

    vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = find_memory_type(physical_device, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) {
        vkDestroyBuffer(device, staging_buffer, nullptr);
        vkFreeMemory(device, staging_buffer_memory, nullptr);
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }
    vkBindImageMemory(device, tex.image, tex.memory, 0);

    // Record copy
    VkCommandBufferAllocateInfo cmd_alloc_info{};
    cmd_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc_info.commandPool = command_pool;
    cmd_alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &cmd_alloc_info, &cmd);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_count;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 6;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    std::vector<VkBufferImageCopy> copy_regions;
    for (uint32_t m = 0; m < mip_count; ++m) {
        for (uint32_t f = 0; f < 6; ++f) {
            const auto& face = all_mips[m][f];
            VkBufferImageCopy region{};
            region.bufferOffset = face.buffer_offset;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = m;
            region.imageSubresource.baseArrayLayer = f;
            region.imageSubresource.layerCount = 1;
            region.imageExtent = { static_cast<uint32_t>(face.width), static_cast<uint32_t>(face.height), 1 };
            copy_regions.push_back(region);
        }
    }

    vkCmdCopyBufferToImage(cmd, staging_buffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(copy_regions.size()), copy_regions.data());

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;
    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);
    vkFreeCommandBuffers(device, command_pool, 1, &cmd);

    vkDestroyBuffer(device, staging_buffer, nullptr);
    vkFreeMemory(device, staging_buffer_memory, nullptr);

    // Create Cube ImageView
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = tex.image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = mip_count;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;
    if (vkCreateImageView(device, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) {
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }

    // Create Trilinear Clamp-to-Edge Sampler
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = static_cast<float>(mip_count - 1);
    if (vkCreateSampler(device, &sampler_info, nullptr, &tex.sampler) != VK_SUCCESS) {
        return std::unexpected(VulkanError::ALLOCATION_FAILED);
    }

    return tex;
}
    std::expected<Texture, VulkanError> create_depth_texture(
        VkDevice device,
        VkPhysicalDevice physical_device,
        uint32_t width,
        uint32_t height
    ) {
        Texture tex;
        tex.device = device;
        
        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = VK_FORMAT_D32_SFLOAT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(device, &image_info, nullptr, &tex.image) != VK_SUCCESS) {
            return std::unexpected(VulkanError::ALLOCATION_FAILED);
        }

        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);

        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);

        uint32_t memory_type_index = -1;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
            if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                (mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memory_type_index = i;
                break;
            }
        }

        if (memory_type_index == static_cast<uint32_t>(-1)) {
            return std::unexpected(VulkanError::ALLOCATION_FAILED);
        }

        VkMemoryAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memory_type_index;

        if (vkAllocateMemory(device, &alloc_info, nullptr, &tex.memory) != VK_SUCCESS) {
            return std::unexpected(VulkanError::ALLOCATION_FAILED);
        }

        vkBindImageMemory(device, tex.image, tex.memory, 0);

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = tex.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_D32_SFLOAT;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &view_info, nullptr, &tex.image_view) != VK_SUCCESS) {
            return std::unexpected(VulkanError::ALLOCATION_FAILED);
        }

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.mipLodBias = 0.0f;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 1.0f;

        if (vkCreateSampler(device, &sampler_info, nullptr, &tex.sampler) != VK_SUCCESS) {
            return std::unexpected(VulkanError::ALLOCATION_FAILED);
        }

        return tex;
    }

    void BindlessManager::write_texture(
    VkDevice device,
    VkDescriptorSet set,
    uint32_t texture_binding,
    uint32_t index,
    const Texture& texture)
{

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView   = texture.get_view();

    VkWriteDescriptorSet image_write{};
    image_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    image_write.dstSet          = set;
    image_write.dstBinding      = texture_binding; // 0
    image_write.dstArrayElement = index;
    image_write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    image_write.descriptorCount = 1;
    image_write.pImageInfo      = &image_info;

    uint32_t sampler_binding = 2; // Binding 2 is defaultSampler

    VkDescriptorImageInfo sampler_info{};
    sampler_info.sampler = texture.get_sampler();

    VkWriteDescriptorSet sampler_write{};
    sampler_write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sampler_write.dstSet          = set;
    sampler_write.dstBinding      = sampler_binding;
    sampler_write.dstArrayElement = 0;
    sampler_write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    sampler_write.descriptorCount = 1;
    sampler_write.pImageInfo      = &sampler_info;

    std::array<VkWriteDescriptorSet, 2> writes = { image_write, sampler_write };
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

}  // namespace vanta::vulkan

