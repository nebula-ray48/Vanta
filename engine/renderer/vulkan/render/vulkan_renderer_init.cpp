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
            .dstBinding = 0,
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
            .dstBinding = 1,
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
    auto ubo_layout_opt = create_global_ubo_layout(context_.device);
    if (!ubo_layout_opt) return std::unexpected(ubo_layout_opt.error());
    ubo_layout_ = *ubo_layout_opt;

    auto ubo_pool_opt = create_descriptor_pool(context_);
    if (!ubo_pool_opt) return std::unexpected(ubo_pool_opt.error());
    ubo_pool_ = *ubo_pool_opt;

    auto ubo_set_opt = create_descriptor_set(
        context_, ubo_pool_, ubo_layout_, global_ubo_buffer_.buffer);
    if (!ubo_set_opt) return std::unexpected(ubo_set_opt.error());
    global_ubo_set_ = *ubo_set_opt;

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

    std::array<VkDescriptorSetLayout, 2> const layouts = {
        ubo_layout_,       // Set 0
        bindless_layout_   // Set 1
    };

    auto pipeline = GraphicsPipeline::create(
        context_.device,
        swapchain_target_.format,
        VK_FORMAT_UNDEFINED,
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
    auto vertex_buffer = create_device_local_buffer<Vertex>(
        context_,
        frames_[current_frame_index_].graphics_command_pool,
        std::span(data.vertices),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (!vertex_buffer) {
        return std::unexpected(vertex_buffer.error());
    }

    auto index_buffer = create_device_local_buffer<uint32_t>(
        context_,
        frames_[current_frame_index_].graphics_command_pool,
        std::span(data.indices),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    if (!index_buffer) {
        vertex_buffer->destroy(context_);
        return std::unexpected(index_buffer.error());
    }

    const MeshId mesh_id{static_cast<uint32_t>(meshes_.size())};
    meshes_.push_back(GpuMesh{
        .vertex_buffer = std::move(*vertex_buffer),
        .index_buffer = std::move(*index_buffer),
        .index_count = static_cast<uint32_t>(data.indices.size()),
    });

    return mesh_id;
}

}  // namespace vanta::render

