//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <iostream>
#include <utility>

#include "scene/gltf_loader.h"
#include "vulkan/resources/buffers/buffer.h"
#include "vulkan_renderer.h"

namespace vanta::render {

VulkanRenderer::VulkanRenderer(VulkanRenderer&& other) noexcept {
    *this = std::move(other);
}

VulkanRenderer& VulkanRenderer::operator=(VulkanRenderer&& other) noexcept {
    if (this != &other) {
        if (context_.device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(context_.device);
        }

        context_ = other.context_;
        swapchain_target_ = std::move(other.swapchain_target_);
        sync_ = std::move(other.sync_);
        meshes_ = std::move(other.meshes_);
        global_ubo_buffer_ = std::move(other.global_ubo_buffer_);
        descriptor_pool_ = other.descriptor_pool_;
        descriptor_set_layout_ = other.descriptor_set_layout_;
        global_descriptor_set_ = other.global_descriptor_set_;
        pipeline_ = std::move(other.pipeline_);

        ubo_layout_ = other.ubo_layout_;
        ubo_pool_ = other.ubo_pool_;
        global_ubo_set_ = other.global_ubo_set_;

        bindless_layout_ = other.bindless_layout_;
        bindless_pool_ = other.bindless_pool_;
        global_bindless_set_ = other.global_bindless_set_;

        other.context_.device = VK_NULL_HANDLE;

        other.ubo_layout_ = VK_NULL_HANDLE;
        other.ubo_pool_ = VK_NULL_HANDLE;
        other.bindless_layout_ = VK_NULL_HANDLE;
        other.bindless_pool_ = VK_NULL_HANDLE;
    }
    return *this;
}

VulkanRenderer::~VulkanRenderer() {
    if (context_.device == VK_NULL_HANDLE) {
        return;
    }

    vkDeviceWaitIdle(context_.device);

    for (auto& mesh : meshes_) {
        mesh.vertex_buffer.destroy(context_);
        mesh.index_buffer.destroy(context_);
    }
    meshes_.clear();

    swapchain_target_.destroy(context_.device);
    pipeline_.destroy(context_.device);
    sync_.destroy(context_.device);

    global_ubo_buffer_.destroy(context_);

    if (ubo_pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(context_.device, ubo_pool_, nullptr);
        ubo_pool_ = VK_NULL_HANDLE;
    }
    if (ubo_layout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(context_.device, ubo_layout_, nullptr);
        ubo_layout_ = VK_NULL_HANDLE;
    }

    if (bindless_pool_ != VK_NULL_HANDLE) {
        BindlessDescriptorManager::destroy_pool(context_.device, bindless_pool_);
        bindless_pool_ = VK_NULL_HANDLE;
    }
    if (bindless_layout_ != VK_NULL_HANDLE) {
        BindlessDescriptorLayout::destroy(context_.device, bindless_layout_);
        bindless_layout_ = VK_NULL_HANDLE;
    }

    context_.destroy();
    std::cout << "VulkanRenderer child objects destroyed cleanly.\n";
}

std::expected<VulkanRenderer, EngineError> VulkanRenderer::create(
    const char* app_name,
    void* window_handle,
    uint32_t window_width,
    uint32_t window_height) {
    VulkanRenderer renderer;

    auto context = create_vulkan_context(app_name, window_handle);
    if (!context) {
        return std::unexpected(context.error());
    }
    renderer.context_ = std::move(*context);

    auto swapchain = create_swapchain_target(renderer.context_, window_width, window_height);
    if (!swapchain) {
        return std::unexpected(swapchain.error());
    }
    renderer.swapchain_target_ = std::move(*swapchain);

    auto sync = SyncContext::create(
        renderer.context_.device,
        renderer.context_.graphics_queue_family_index,
        static_cast<uint32_t>(renderer.swapchain_target_.images.size()));
    if (!sync) {
        return std::unexpected(sync.error());
    }
    renderer.sync_ = std::move(*sync);

    constexpr VmaAllocationCreateInfo ubo_alloc_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    auto ubo_buffer = create_buffer(
        renderer.context_,
        sizeof(GlobalUbo),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        ubo_alloc_info);
    if (!ubo_buffer) {
        return std::unexpected(ubo_buffer.error());
    }
    renderer.global_ubo_buffer_ = std::move(*ubo_buffer);

    if (auto descriptor_result = renderer.initialize_descriptor_resources(); !descriptor_result) {
        return std::unexpected(descriptor_result.error());
    }

    if (auto pipeline_result = renderer.initialize_pipeline_resources(); !pipeline_result) {
        return std::unexpected(pipeline_result.error());
    }

    if (auto texture_result = renderer.initialize_textures(); !texture_result) {
        return std::unexpected(texture_result.error());
    }

    return renderer;
}

    std::expected<void, std::string> vanta::render::VulkanRenderer::load_scene(const std::string& filepath) {
    auto scene_result = vanta::scene::load_gltf(filepath);
    if (!scene_result.has_value()) {
        return std::unexpected("glTF load failed: " + std::to_string(std::to_underlying(scene_result.error())));
    }
    const auto& scene = scene_result.value();

    size_t vertex_size = scene.vertices.size() * sizeof(scene.vertices[0]);
    auto vertex_res = upload_buffer_to_gpu(
        context_.allocator, context_.device, sync_.command_pool, context_.graphics_queue,
        vertex_size, scene.vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
    if (!vertex_res.has_value()) {
        return std::unexpected("Vertex upload failed: " + vertex_res.error());
    }
    vertex_buffer_ = vertex_res.value();

    size_t index_size = scene.indices.size() * sizeof(scene.indices[0]);
    auto index_res = upload_buffer_to_gpu(
        context_.allocator, context_.device, sync_.command_pool, context_.graphics_queue,
        index_size, scene.indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    );
    if (!index_res.has_value()) {
        vmaDestroyBuffer(context_.allocator, vertex_buffer_.buffer, vertex_buffer_.allocation);
        return std::unexpected("Index upload failed: " + index_res.error());
    }
    index_buffer_ = index_res.value();
    index_count_ = static_cast<uint32_t>(scene.indices.size());

    return {};
}

}  // namespace vanta::render

