//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <array>
#include <iostream>
#include <utility>

#include "assets/image_loader.hpp"
#include "vulkan/resources/buffers/buffer.h"
#include "vulkan/resources/descriptors/descriptor.h"
#include "vulkan/resources/images/texture.h"
#include "vulkan_renderer.h"

namespace vanta::render {
namespace {
void update_bindless_texture(
VkDevice device, VkDescriptorSet bindless_set,
uint32_t binding, uint32_t index, const vanta::vulkan::Texture& texture)
{
    const VkDescriptorImageInfo tex_info{
        .sampler = VK_NULL_HANDLE,
        .imageView = texture.get_view(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    const VkWriteDescriptorSet write_tex{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bindless_set,
        .dstBinding = 1,
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = &tex_info,
    };

    const VkDescriptorImageInfo sampler_info{
        .sampler = texture.get_sampler(),
        .imageView = VK_NULL_HANDLE,
        .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    const VkWriteDescriptorSet write_sampler{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = bindless_set,
        .dstBinding = 2,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &sampler_info,
    };

    const std::array writes = {write_tex, write_sampler};
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
} // namespace

std::expected<void, EngineError> VulkanRenderer::initialize_textures() {

    auto image_data_opt = load_image("assets/textures/painted_plaster_wall_diff_4k.jpg");
    if (!image_data_opt) {
        return std::unexpected(EngineError{LegacyError{"テクスチャ画像のロードに失敗しました"}});
    }

    auto texture_opt = vanta::vulkan::create_texture_from_image(
        context_.device,
        context_.physical_device,
        frames_[current_frame_index_].graphics_command_pool,
        context_.graphics_queue,
        *image_data_opt
    );

    if (!texture_opt) {
        return std::unexpected(EngineError{LegacyError{"テクスチャのVRAM転送に失敗しました"}});
    }

    textures_.push_back(std::move(*texture_opt));
    const uint32_t texture_index = static_cast<uint32_t>(textures_.size() - 1);

    update_bindless_texture(
        context_.device,
        global_bindless_set_,
        0,
        texture_index,
        textures_.back()
    );

    std::cout << "Bindless texture registered at index: " << texture_index << "\n";
    return {};
}

std::expected<void, EngineError> VulkanRenderer::initialize_descriptor_resources() {
    auto bindless_layout_opt = BindlessDescriptorLayout::create(context_.device);
    if (!bindless_layout_opt) return std::unexpected(bindless_layout_opt.error());
    bindless_layout_ = *bindless_layout_opt;

    auto bindless_pool_opt = BindlessDescriptorManager::create_pool(context_.device);
    if (!bindless_pool_opt) return std::unexpected(bindless_pool_opt.error());
    bindless_pool_ = *bindless_pool_opt;

    auto bindless_set_opt = BindlessDescriptorManager::allocate_set(
        context_.device, bindless_pool_, bindless_layout_);
    if (!bindless_set_opt) return std::unexpected(bindless_set_opt.error());
    global_bindless_set_ = *bindless_set_opt;

    BindlessDescriptorManager::update_ubo(
        context_.device,
        global_bindless_set_,
        global_ubo_buffer_.buffer,
        sizeof(GlobalUbo)
    );

    const VkDescriptorBufferInfo object_info{
        .buffer = object_buffer_.buffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    const VkWriteDescriptorSet write_object{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = global_bindless_set_,
        .dstBinding = 3,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &object_info,
    };
    vkUpdateDescriptorSets(context_.device, 1, &write_object, 0, nullptr);

    return {};
}

std::expected<void, EngineError> VulkanRenderer::initialize_pipeline_resources() {
    constexpr VkVertexInputBindingDescription binding_description{
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    constexpr std::array attribute_descriptions = {
        VkVertexInputAttributeDescription{
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, position),
        },
        VkVertexInputAttributeDescription{
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, color),
        },
        VkVertexInputAttributeDescription{
            .location = 2,
            .binding = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = offsetof(Vertex, normal),
        },
        VkVertexInputAttributeDescription{
            .location = 3,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = offsetof(Vertex, uv),
        },
        VkVertexInputAttributeDescription{
            .location = 4,
            .binding = 0,
            .format = VK_FORMAT_R32_UINT,
            .offset = offsetof(Vertex, texture_id),
        },
    };

    std::array<VkDescriptorSetLayout, 1> const layouts = {
        bindless_layout_   // Set 0
    };

    auto pipeline = GraphicsPipeline::create(
        context_.device,
        swapchain_target_.format,
        VK_FORMAT_D32_SFLOAT,
        swapchain_target_.extent,
        layouts,
        binding_description,
        attribute_descriptions);

    if (!pipeline) {
        return std::unexpected(pipeline.error());
    }

    pipeline_ = std::move(*pipeline);
    return {};
}

std::expected<MeshId, EngineError> VulkanRenderer::create_mesh_from_data(const MeshData& data) {
    size_t vertex_data_size = data.vertices.size() * sizeof(Vertex);
    size_t index_data_size = data.indices.size() * sizeof(uint32_t);

    // TODO: バッファサイズのオーバーフローチェック (Phase 1なので今回は省略)

    void* mapped_vertex = nullptr;
    vmaMapMemory(context_.allocator, global_vertex_buffer_.allocation, &mapped_vertex);
    uint8_t* vertex_dst = static_cast<uint8_t*>(mapped_vertex) + (global_vertex_count_ * sizeof(Vertex));
    std::memcpy(vertex_dst, data.vertices.data(), vertex_data_size);
    vmaUnmapMemory(context_.allocator, global_vertex_buffer_.allocation);

    void* mapped_index = nullptr;
    vmaMapMemory(context_.allocator, global_index_buffer_.allocation, &mapped_index);
    uint8_t* index_dst = static_cast<uint8_t*>(mapped_index) + (global_index_count_ * sizeof(uint32_t));
    std::memcpy(index_dst, data.indices.data(), index_data_size);
    vmaUnmapMemory(context_.allocator, global_index_buffer_.allocation);

    const MeshId mesh_id{static_cast<uint32_t>(meshes_.size())};
    meshes_.push_back(GpuMesh{
        .first_index = global_index_count_,
        .index_count = static_cast<uint32_t>(data.indices.size()),
        .vertex_offset = static_cast<int32_t>(global_vertex_count_),
    });

    global_vertex_count_ += static_cast<uint32_t>(data.vertices.size());
    global_index_count_ += static_cast<uint32_t>(data.indices.size());

    return mesh_id;
}

std::expected<void, EngineError> VulkanRenderer::initialize_draw_buffers() {
    constexpr VmaAllocationCreateInfo alloc_info {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    constexpr size_t max_objects = 10000;
    auto obj_buffer = create_buffer(
            context_,
            sizeof(GpuObjectData) * max_objects,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            alloc_info
    );
    if (!obj_buffer) {
        return std::unexpected(obj_buffer.error());
    }
    object_buffer_ = std::move(*obj_buffer);

    // インダイレクト描画指示のバッファ作成
    auto indirect_buffer = create_buffer(
        context_,
        sizeof(VkDrawIndexedIndirectCommand) * max_objects,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        alloc_info
    );
    if (!indirect_buffer) {
        return std::unexpected(indirect_buffer.error());
    }
    indirect_buffer_ = std::move(*indirect_buffer);

    constexpr size_t BYTE_SIZE = 64 * 1024 * 1024;
    auto vertex_buffer = create_buffer(
        context_,
        BYTE_SIZE,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        alloc_info
    );
    if (!vertex_buffer) return std::unexpected(vertex_buffer.error());
    global_vertex_buffer_ = std::move(*vertex_buffer);

    constexpr size_t BYTE_SIZE16MB = 16 * 1024 * 1024;
    auto index_buffer = create_buffer(
        context_,
        BYTE_SIZE16MB,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        alloc_info
    );
    if (!index_buffer) return std::unexpected(index_buffer.error());
    global_index_buffer_ = std::move(*index_buffer);

    return {};
}

}  // namespace vanta::render

