//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <iostream>
#include <utility>

#include "assets/gltf_loader.h"
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
        frames_ = std::move(other.frames_);
        meshes_ = std::move(other.meshes_);
        registry_ = std::move(other.registry_);
        swapchain_image_handles_ = std::move(other.swapchain_image_handles_);
        global_ubo_buffer_ = std::move(other.global_ubo_buffer_);
        pipeline_ = std::move(other.pipeline_);
        bindless_layout_ = other.bindless_layout_;
        bindless_pool_ = other.bindless_pool_;
        global_bindless_set_ = other.global_bindless_set_;

        other.context_.device = VK_NULL_HANDLE;

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

    for (auto& frame : frames_) {
        frame.destroy(context_);
    }
    swapchain_target_.destroy(context_.device);
    pipeline_.destroy(context_.device);

    global_ubo_buffer_.destroy(context_);

    if (bindless_pool_ != VK_NULL_HANDLE) {
        BindlessDescriptorManager::destroy_pool(context_.device, bindless_pool_);
        bindless_pool_ = VK_NULL_HANDLE;
    }
    if (bindless_layout_ != VK_NULL_HANDLE) {
        BindlessDescriptorLayout::destroy(context_.device, bindless_layout_);
        bindless_layout_ = VK_NULL_HANDLE;
    }

    registry_.clear_pool(context_);

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

    for (size_t i = 0; i < renderer.swapchain_target_.images.size(); ++i) {
        auto handle = renderer.registry_.register_imported_image(
            renderer.swapchain_target_.images[i],
            renderer.swapchain_target_.image_views[i],
            ImageDescription{
                .width = renderer.swapchain_target_.extent.width,
                .height = renderer.swapchain_target_.extent.height,
                .format = renderer.swapchain_target_.format,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .ownership = ResourceOwnership::IMPORTED,
            }
        );
        renderer.swapchain_image_handles_.push_back(handle);
    }

    for (uint32_t index = 0; index < MAX_FRAMES_IN_FLIGHT; ++index) {
        if (auto frame_result = renderer.frames_[index].initialize(renderer.context_, index);
            !frame_result) {
            return std::unexpected(frame_result.error());
        }
    }

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
        context_.allocator, context_.device,
        frames_[current_frame_index_].graphics_command_pool, context_.graphics_queue,
        vertex_size, scene.vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    );
    if (!vertex_res.has_value()) {
        return std::unexpected("Vertex upload failed: " + vertex_res.error());
    }
    vertex_buffer_ = vertex_res.value();

    size_t index_size = scene.indices.size() * sizeof(scene.indices[0]);
    auto index_res = upload_buffer_to_gpu(
        context_.allocator, context_.device,
        frames_[current_frame_index_].graphics_command_pool, context_.graphics_queue,
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

