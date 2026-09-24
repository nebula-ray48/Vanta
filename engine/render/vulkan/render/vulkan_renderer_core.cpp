//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <iostream>
#include <utility>

#include "assets/gltf_loader.h"
#include "assets/image_loader.hpp"
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
        pipeline_layout_ = other.pipeline_layout_;
        pbr_pipeline_ = std::move(other.pbr_pipeline_);
        toon_pipeline_ = std::move(other.toon_pipeline_);
        toon_outline_pipeline_ = std::move(other.toon_outline_pipeline_);
        skybox_pipeline_ = std::move(other.skybox_pipeline_);
        shadow_pipeline_ = std::move(other.shadow_pipeline_);
        bindless_layout_ = other.bindless_layout_;
        bindless_pool_ = other.bindless_pool_;
        global_bindless_set_ = other.global_bindless_set_;
        object_buffer_ = std::move(other.object_buffer_);
        indirect_buffer_ = std::move(other.indirect_buffer_);
        shadow_map_handle_ = std::move(other.shadow_map_handle_);
        shadow_map_index_ = other.shadow_map_index_;

        global_vertex_buffer_ = std::move(other.global_vertex_buffer_);
        global_index_buffer_ = std::move(other.global_index_buffer_);
        global_vertex_count_ = other.global_vertex_count_;
        global_index_count_ = other.global_index_count_;
        index_count_ = other.index_count_;
        textures_ = std::move(other.textures_);
        env_cubemap_ = std::move(other.env_cubemap_);
        brdf_lut_index_ = other.brdf_lut_index_;

        other.context_.device = VK_NULL_HANDLE;
        other.pipeline_layout_ = VK_NULL_HANDLE;
        other.bindless_layout_ = VK_NULL_HANDLE;
        other.bindless_pool_ = VK_NULL_HANDLE;
        other.global_bindless_set_ = VK_NULL_HANDLE;
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
    
    pbr_pipeline_.destroy(context_.device);
    toon_pipeline_.destroy(context_.device);
    toon_outline_pipeline_.destroy(context_.device);
    skybox_pipeline_.destroy(context_.device);
    shadow_pipeline_.destroy(context_.device);
    if (pipeline_layout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(context_.device, pipeline_layout_, nullptr);
        pipeline_layout_ = VK_NULL_HANDLE;
    }

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
    textures_.clear();
    env_cubemap_.reset();

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

std::expected<std::vector<vanta::render::VulkanRenderer::LoadedSceneNode>, std::string> vanta::render::VulkanRenderer::load_scene(const std::string& filepath) {
    auto scene_result = vanta::scene::load_gltf(filepath);
    if (!scene_result.has_value()) {
        return std::unexpected("glTF load failed: " + std::to_string(std::to_underlying(scene_result.error())));
    }
    const auto& scene = scene_result.value();

    // 1. 画像のロード
    std::vector<uint32_t> loaded_texture_ids;
    for (const auto& img_data : scene.images) {
        uint32_t current_id = 0; // fallback
        std::expected<RawImage, TextureError> raw_img_opt = std::unexpected(TextureError::LoadFailed);
        
        if (!img_data.uri.empty()) {
            auto tex_path = std::filesystem::path(filepath).parent_path() / img_data.uri;
            raw_img_opt = load_image(tex_path);
            if (!raw_img_opt) {
                std::cerr << "Failed to load texture file: " << tex_path << "\n";
            }
        } else if (!img_data.raw_data.empty()) {
            raw_img_opt = load_image_from_memory(img_data.raw_data.data(), img_data.raw_data.size());
            if (!raw_img_opt) {
                std::cerr << "Failed to load texture from memory for image: " << img_data.name << "\n";
            }
        } else {
            std::cerr << "Image data has neither URI nor raw_data: " << img_data.name << "\n";
        }

        if (raw_img_opt) {
                auto tex_opt = vanta::vulkan::create_texture_from_image(
                    context_.device,
                    context_.physical_device,
                    frames_[0].graphics_command_pool,
                    context_.graphics_queue,
                    *raw_img_opt
                );
                
                if (tex_opt) {
                    auto tex_id_opt = register_texture(std::move(*tex_opt));
                    if (tex_id_opt) {
                        current_id = *tex_id_opt;
                        std::cout << "Loaded GLTF texture: " << img_data.name << " ID: " << current_id << "\n";
                    }
                }
            }
        
        loaded_texture_ids.push_back(current_id); 
    }

    // 2. マテリアルの変換
    std::vector<MaterialData> loaded_materials;
    for (const auto& mat : scene.materials) {
        MaterialData mat_data{};
        mat_data.type = MaterialType::PBR;
        mat_data.pbr.base_color = mat.base_color_factor;
        mat_data.pbr.metallic = mat.metallic_factor;
        mat_data.pbr.roughness = mat.roughness_factor;
        
        mat_data.pbr.albedo_texture_id = mat.base_color_texture_index >= 0 && mat.base_color_texture_index < loaded_texture_ids.size() 
            ? loaded_texture_ids[mat.base_color_texture_index] : 0;
            
        mat_data.pbr.normal_texture_id = mat.normal_texture_index >= 0 && mat.normal_texture_index < loaded_texture_ids.size() 
            ? loaded_texture_ids[mat.normal_texture_index] : 0;
            
        mat_data.pbr.mrm_texture_id = mat.metallic_roughness_texture_index >= 0 && mat.metallic_roughness_texture_index < loaded_texture_ids.size() 
            ? loaded_texture_ids[mat.metallic_roughness_texture_index] : 0;

        loaded_materials.push_back(mat_data);
    }

    // 3. メッシュとノードの作成
    size_t vertex_data_size = scene.vertices.size() * sizeof(Vertex);
    size_t index_data_size = scene.indices.size() * sizeof(uint32_t);

    if (vertex_data_size > 0) {
        void* mapped_vertex = nullptr;
        vmaMapMemory(context_.allocator, global_vertex_buffer_.allocation, &mapped_vertex);
        uint8_t* vertex_dst = static_cast<uint8_t*>(mapped_vertex) + (global_vertex_count_ * sizeof(Vertex));
        
        std::vector<Vertex> render_vertices(scene.vertices.size());
        for (size_t i = 0; i < scene.vertices.size(); ++i) {
            render_vertices[i].position = scene.vertices[i].position;
            render_vertices[i].color = glm::vec3(1.0f);
            render_vertices[i].normal = scene.vertices[i].normal;
            render_vertices[i].uv = scene.vertices[i].uv;
            render_vertices[i].texture_id = 0; 
        }
        std::memcpy(vertex_dst, render_vertices.data(), vertex_data_size);
        vmaUnmapMemory(context_.allocator, global_vertex_buffer_.allocation);
    }

    if (index_data_size > 0) {
        void* mapped_index = nullptr;
        vmaMapMemory(context_.allocator, global_index_buffer_.allocation, &mapped_index);
        uint8_t* index_dst = static_cast<uint8_t*>(mapped_index) + (global_index_count_ * sizeof(uint32_t));
        std::memcpy(index_dst, scene.indices.data(), index_data_size);
        vmaUnmapMemory(context_.allocator, global_index_buffer_.allocation);
    }

    std::vector<MeshId> primitive_meshes;
    for (const auto& prim : scene.primitives) {
        const MeshId mesh_id{static_cast<uint32_t>(meshes_.size())};
        meshes_.push_back(GpuMesh{
            .first_index = global_index_count_ + prim.first_index,
            .index_count = prim.index_count,
            .vertex_offset = static_cast<int32_t>(global_vertex_count_ + prim.vertex_offset),
        });
        primitive_meshes.push_back(mesh_id);
    }

    global_vertex_count_ += static_cast<uint32_t>(scene.vertices.size());
    global_index_count_ += static_cast<uint32_t>(scene.indices.size());

    std::vector<LoadedSceneNode> nodes;

    auto traverse = [&](auto& self, uint32_t node_idx, const glm::mat4& parent_transform) -> void {
        const auto& node = scene.nodes[node_idx];
        glm::mat4 global_transform = parent_transform * node.local_transform;

        if (node.mesh_index >= 0 && node.mesh_index < scene.meshes.size()) {
            const auto& mesh = scene.meshes[node.mesh_index];
            for (uint32_t i = 0; i < mesh.primitive_count; ++i) {
                uint32_t prim_idx = mesh.first_primitive + i;
                if (prim_idx < scene.primitives.size()) {
                    const auto& prim = scene.primitives[prim_idx];
                    LoadedSceneNode loaded_node{};
                    loaded_node.mesh_id = primitive_meshes[prim_idx];
                    loaded_node.global_transform = global_transform;

                    if (prim.material_index >= 0 && prim.material_index < loaded_materials.size()) {
                        loaded_node.material = loaded_materials[prim.material_index];
                    } else if (!loaded_materials.empty()) {
                        loaded_node.material = loaded_materials[0];
                    } else {
                        loaded_node.material = MaterialData{};
                        loaded_node.material.type = MaterialType::PBR;
                        loaded_node.material.pbr = PbrMaterialParams{ .base_color = glm::vec4(1.0f), .metallic = 0.0f, .roughness = 1.0f };
                    }
                    nodes.push_back(loaded_node);
                }
            }
        }

        for (uint32_t child_idx : node.children) {
            self(self, child_idx, global_transform);
        }
    };

    for (uint32_t root_idx : scene.root_nodes) {
        traverse(traverse, root_idx, glm::mat4(1.0f));
    }

    return nodes;
}

std::expected<uint32_t, EngineError> vanta::render::VulkanRenderer::register_texture(vanta::vulkan::Texture&& texture) {
    uint32_t index = static_cast<uint32_t>(textures_.size());
    
    // Binding 1 は bindlessTextures[]
    vanta::vulkan::BindlessManager::write_texture(
        context_.device,
        global_bindless_set_,
        1,
        index,
        texture
    );
    
    textures_.push_back(std::move(texture));
    return index;
}

}  // namespace vanta::render

