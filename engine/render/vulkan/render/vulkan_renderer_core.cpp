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
        object_buffer_ = std::move(other.object_buffer_);
        indirect_buffer_ = std::move(other.indirect_buffer_);

        global_vertex_buffer_ = std::move(other.global_vertex_buffer_);
        global_index_buffer_ = std::move(other.global_index_buffer_);
        global_vertex_count_ = other.global_vertex_count_;
        global_index_count_ = other.global_index_count_;

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

    meshes_.clear();

    for (auto& frame : frames_) {
        frame.destroy(context_);
    }
    swapchain_target_.destroy(context_.device);
    pipeline_.destroy(context_.device);

    global_ubo_buffer_.destroy(context_);
    object_buffer_.destroy(context_);
    indirect_buffer_.destroy(context_);
    global_vertex_buffer_.destroy(context_);
    global_index_buffer_.destroy(context_);

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

    if (auto draw_result = renderer.initialize_draw_buffers(); !draw_result) {
        return std::unexpected(draw_result.error());
    }

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

    std::vector<Vertex> render_vertices(scene.vertices.size());
    std::memcpy(render_vertices.data(), scene.vertices.data(), scene.vertices.size() * sizeof(Vertex));

    MeshData data { std::move(render_vertices), scene.indices };
    auto mesh_result = create_mesh_from_data(data);
    if (!mesh_result) {
        return std::unexpected("Mesh creation failed.");
    }

    return {};
}

}  // namespace vanta::render

