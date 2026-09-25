//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <array>
#include <iostream>
#include <utility>
#include <random>
#include <cmath>

#include "assets/image_loader.hpp"
#include "vulkan/resources/buffers/buffer.h"
#include "vulkan/resources/descriptors/descriptor.h"
#include "vulkan/resources/images/texture.h"
#include "vulkan_renderer.h"
#include "ext/glfw3.h"

#include "imgui.h"
#include "ext/imgui_impl_glfw.h"
#include "ext/imgui_impl_vulkan.h"

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
    // 0: デフォルトテクスチャのロード
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
    uint32_t default_texture_index = static_cast<uint32_t>(textures_.size() - 1);

    update_bindless_texture(
        context_.device,
        global_bindless_set_,
        0,
        default_texture_index,
        textures_.back()
    );

    std::cout << "Default texture registered at index: " << default_texture_index << "\n";

    // 1: BRDF LUT (2D UNORM) のロード
    auto lut_image_opt = load_image("assets/textures/ibl/brdf_lut.png");
    if (lut_image_opt) {
        auto lut_texture_opt = vanta::vulkan::create_texture_from_image(
            context_.device,
            context_.physical_device,
            frames_[current_frame_index_].graphics_command_pool,
            context_.graphics_queue,
            *lut_image_opt,
            VK_FORMAT_R8G8B8A8_UNORM
        );

        if (lut_texture_opt) {
            textures_.push_back(std::move(*lut_texture_opt));
            brdf_lut_index_ = static_cast<uint32_t>(textures_.size() - 1);
            update_bindless_texture(
                context_.device,
                global_bindless_set_,
                0,
                brdf_lut_index_,
                textures_.back()
            );
            std::cout << "BRDF LUT registered at index: " << brdf_lut_index_ << "\n";
        } else {
            std::cerr << "Warning: Failed to upload BRDF LUT to GPU\n";
        }
    } else {
        std::cerr << "Warning: Could not load assets/textures/ibl/brdf_lut.png\n";
    }

    // 2: IBL Specular キューブマップのロード
    auto cubemap_opt = vanta::vulkan::create_cubemap_from_hdr_mips(
        context_.device,
        context_.physical_device,
        frames_[current_frame_index_].graphics_command_pool,
        context_.graphics_queue,
        "assets/textures/ibl/studio_small_04_4k",
        6
    );

    if (cubemap_opt) {
        env_cubemap_ = std::move(*cubemap_opt);
        BindlessDescriptorManager::update_cubemap(
            context_.device,
            global_bindless_set_,
            env_cubemap_->get_view(),
            env_cubemap_->get_sampler()
        );
        std::cout << "IBL Cubemap successfully loaded and bound to descriptor set\n";
    } else {
        std::cerr << "Warning: Failed to load IBL Cubemap from assets/textures/ibl/studio_small_04_4k\n";
    }

    // 3: Shadow Map texture creation
    auto shadow_tex_opt = vanta::vulkan::create_depth_texture(
        context_.device,
        context_.physical_device,
        2048, 2048
    );
    if (shadow_tex_opt) {
        textures_.push_back(std::move(*shadow_tex_opt));
        shadow_map_index_ = static_cast<uint32_t>(textures_.size() - 1);

        shadow_map_handle_ = registry_.register_imported_image(
            textures_.back().get_image(),
            textures_.back().get_view(),
            ImageDescription{
                .width = 2048,
                .height = 2048,
                .format = VK_FORMAT_D32_SFLOAT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                .ownership = ResourceOwnership::IMPORTED,
            }
        );

        update_bindless_texture(
            context_.device,
            global_bindless_set_,
            0,
            shadow_map_index_,
            textures_.back()
        );
        std::cout << "Shadow map registered at index: " << shadow_map_index_ << "\n";
    } else {
        std::cerr << "Warning: Failed to create shadow map\n";
    }

    // 4: SSAO Random Noise Texture & Samples
    {
        // 64 samples
        std::uniform_real_distribution<float> random_floats(0.0, 1.0); // 0 to 1
        std::default_random_engine generator;
        for (uint32_t i = 0; i < 64; ++i) {
            glm::vec3 sample(
                random_floats(generator) * 2.0 - 1.0,
                random_floats(generator) * 2.0 - 1.0,
                random_floats(generator)
            );
            sample = glm::normalize(sample);
            sample *= random_floats(generator);
            // scale samples so they're more aligned to center of kernel
            float scale = (float)i / 64.0f;
            scale = std::lerp(0.1f, 1.0f, scale * scale);
            sample *= scale;
            ssao_samples_[i] = glm::vec4(sample, 0.0f);
        }

        // Noise Texture 4x4
        auto* data_ptr = static_cast<uint8_t*>(malloc(16 * 4));
        for (uint32_t i = 0; i < 16; i++) {
            glm::vec3 noise(
                random_floats(generator) * 2.0 - 1.0,
                random_floats(generator) * 2.0 - 1.0,
                0.0f);
            noise = glm::normalize(noise);
            data_ptr[i * 4 + 0] = static_cast<uint8_t>((noise.x * 0.5f + 0.5f) * 255.0f);
            data_ptr[i * 4 + 1] = static_cast<uint8_t>((noise.y * 0.5f + 0.5f) * 255.0f);
            data_ptr[i * 4 + 2] = static_cast<uint8_t>((noise.z * 0.5f + 0.5f) * 255.0f);
            data_ptr[i * 4 + 3] = 255;
        }

        RawImage noise_image{
            .width = 4,
            .height = 4,
            .channels = 4,
            .data = StbImagePtr(data_ptr, stbi_image_free)
        };

        auto noise_tex_opt = vanta::vulkan::create_texture_from_image(
            context_.device,
            context_.physical_device,
            frames_[current_frame_index_].graphics_command_pool,
            context_.graphics_queue,
            noise_image,
            VK_FORMAT_R8G8B8A8_UNORM
        );
        if (noise_tex_opt) {
            ssao_noise_tex_ = std::move(*noise_tex_opt);
            std::cout << "SSAO noise texture generated\n";
        }
    }

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

    // --- SSAO Set 1 Descriptor Layout ---
    std::array<VkDescriptorSetLayoutBinding, 4> ssao_bindings = {
        VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };

    VkDescriptorSetLayoutCreateInfo ssao_layout_info{};
    ssao_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ssao_layout_info.bindingCount = static_cast<uint32_t>(ssao_bindings.size());
    ssao_layout_info.pBindings = ssao_bindings.data();

    if (vkCreateDescriptorSetLayout(context_.device, &ssao_layout_info, nullptr, &ssao_layout_) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"SSAO用DescriptorSetLayoutの作成に失敗しました"}});
    }

    std::array<VkDescriptorPoolSize, 2> ssao_pool_sizes = {
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_FRAMES_IN_FLIGHT * 3},
        VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT}
    };

    VkDescriptorPoolCreateInfo ssao_pool_info{};
    ssao_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ssao_pool_info.poolSizeCount = static_cast<uint32_t>(ssao_pool_sizes.size());
    ssao_pool_info.pPoolSizes = ssao_pool_sizes.data();
    ssao_pool_info.maxSets = MAX_FRAMES_IN_FLIGHT;

    if (vkCreateDescriptorPool(context_.device, &ssao_pool_info, nullptr, &ssao_pool_) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"SSAO用DescriptorPoolの作成に失敗しました"}});
    }

    std::vector<VkDescriptorSetLayout> ssao_layouts(MAX_FRAMES_IN_FLIGHT, ssao_layout_);
    VkDescriptorSetAllocateInfo ssao_alloc_info{};
    ssao_alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ssao_alloc_info.descriptorPool = ssao_pool_;
    ssao_alloc_info.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    ssao_alloc_info.pSetLayouts = ssao_layouts.data();

    if (vkAllocateDescriptorSets(context_.device, &ssao_alloc_info, ssao_sets_.data()) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"SSAO用DescriptorSetの割り当てに失敗しました"}});
    }

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
        bindless_layout_,  // Set 0
        ssao_layout_       // Set 1
    };

    VkPushConstantRange const push_constant_range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 128 // 汎用的に128バイト(vec4 x 8個分)確保しておく
    };

    // 1. Pipeline Layout の作成
    VkPipelineLayoutCreateInfo const layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant_range
    };

    if (vkCreatePipelineLayout(context_.device, &layout_info, nullptr, &pipeline_layout_) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"Pipeline Layout生成失敗"}});
    }

    // 2. シェーダーの読み込み
    auto vert_spv = read_shader_file("assets/shaders/main_vert.spv");
    if (!vert_spv) return std::unexpected(vert_spv.error());
    auto frag_spv = read_shader_file("assets/shaders/main_frag.spv");
    if (!frag_spv) return std::unexpected(frag_spv.error());

    auto vert_module = create_shader_module(context_.device, *vert_spv);
    if (!vert_module) return std::unexpected(vert_module.error());
    auto frag_module = create_shader_module(context_.device, *frag_spv);
    if (!frag_module) {
        vkDestroyShaderModule(context_.device, *vert_module, nullptr);
        return std::unexpected(frag_module.error());
    }

    auto skybox_vert_spv = read_shader_file("assets/shaders/skybox_vert.spv");
    if (!skybox_vert_spv) return std::unexpected(skybox_vert_spv.error());
    auto skybox_frag_spv = read_shader_file("assets/shaders/skybox_frag.spv");
    if (!skybox_frag_spv) return std::unexpected(skybox_frag_spv.error());

    auto skybox_vert_module = create_shader_module(context_.device, *skybox_vert_spv);
    if (!skybox_vert_module) return std::unexpected(skybox_vert_module.error());
    auto skybox_frag_module = create_shader_module(context_.device, *skybox_frag_spv);
    if (!skybox_frag_module) return std::unexpected(skybox_frag_module.error());

    auto shadow_vert_spv = read_shader_file("assets/shaders/shadow_vert.spv");
    if (!shadow_vert_spv) return std::unexpected(shadow_vert_spv.error());
    auto shadow_vert_module = create_shader_module(context_.device, *shadow_vert_spv);
    if (!shadow_vert_module) return std::unexpected(shadow_vert_module.error());

    auto depth_normal_vert_spv = read_shader_file("assets/shaders/depth_normal_vert.spv");
    if (!depth_normal_vert_spv) return std::unexpected(depth_normal_vert_spv.error());
    auto depth_normal_frag_spv = read_shader_file("assets/shaders/depth_normal_frag.spv");
    if (!depth_normal_frag_spv) return std::unexpected(depth_normal_frag_spv.error());

    auto tonemap_vert_spv = read_shader_file("assets/shaders/tonemap_vert.spv");
    if (!tonemap_vert_spv) return std::unexpected(tonemap_vert_spv.error());
    auto tonemap_frag_spv = read_shader_file("assets/shaders/tonemap_frag.spv");
    if (!tonemap_frag_spv) return std::unexpected(tonemap_frag_spv.error());
    auto tonemap_vert_module = create_shader_module(context_.device, *tonemap_vert_spv);
    if (!tonemap_vert_module) return std::unexpected(tonemap_vert_module.error());
    auto tonemap_frag_module = create_shader_module(context_.device, *tonemap_frag_spv);
    if (!tonemap_frag_module) return std::unexpected(tonemap_frag_module.error());
    std::vector<VkPipelineShaderStageCreateInfo> tonemap_stages = {
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = *tonemap_vert_module,
                                        .pName  = "main"},
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                                        .module = *tonemap_frag_module,
                                        .pName  = "main"}};

    auto bloom_extract_vert_spv = read_shader_file("assets/shaders/bloom_extract_vert.spv");
    if (!bloom_extract_vert_spv) return std::unexpected(bloom_extract_vert_spv.error());
    auto bloom_extract_frag_spv = read_shader_file("assets/shaders/bloom_extract_frag.spv");
    if (!bloom_extract_frag_spv) return std::unexpected(bloom_extract_frag_spv.error());
    auto bloom_extract_vert_module = create_shader_module(context_.device, *bloom_extract_vert_spv);
    if (!bloom_extract_vert_module) return std::unexpected(bloom_extract_vert_module.error());
    auto bloom_extract_frag_module = create_shader_module(context_.device, *bloom_extract_frag_spv);
    if (!bloom_extract_frag_module) return std::unexpected(bloom_extract_frag_module.error());
    std::vector<VkPipelineShaderStageCreateInfo> bloom_extract_stages = {
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = *bloom_extract_vert_module,
                                        .pName  = "main"},
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                                        .module = *bloom_extract_frag_module,
                                        .pName  = "main"}};

    auto bloom_blur_vert_spv = read_shader_file("assets/shaders/bloom_blur_vert.spv");
    if (!bloom_blur_vert_spv) return std::unexpected(bloom_blur_vert_spv.error());
    auto bloom_blur_frag_spv = read_shader_file("assets/shaders/bloom_blur_frag.spv");
    if (!bloom_blur_frag_spv) return std::unexpected(bloom_blur_frag_spv.error());
    auto bloom_blur_vert_module = create_shader_module(context_.device, *bloom_blur_vert_spv);
    if (!bloom_blur_vert_module) return std::unexpected(bloom_blur_vert_module.error());
    auto bloom_blur_frag_module = create_shader_module(context_.device, *bloom_blur_frag_spv);
    if (!bloom_blur_frag_module) return std::unexpected(bloom_blur_frag_module.error());
    std::vector<VkPipelineShaderStageCreateInfo> bloom_blur_stages = {
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                                        .module = *bloom_blur_vert_module,
                                        .pName  = "main"},
        VkPipelineShaderStageCreateInfo{.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                        .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                                        .module = *bloom_blur_frag_module,
                                        .pName  = "main"}};

    auto depth_normal_vert_module = create_shader_module(context_.device, *depth_normal_vert_spv);
    if (!depth_normal_vert_module) return std::unexpected(depth_normal_vert_module.error());
    auto depth_normal_frag_module = create_shader_module(context_.device, *depth_normal_frag_spv);
    if (!depth_normal_frag_module) return std::unexpected(depth_normal_frag_module.error());

    auto ssao_comp_spv = read_shader_file("assets/shaders/ssao_comp.spv");
    if (!ssao_comp_spv) return std::unexpected(ssao_comp_spv.error());
    auto ssao_comp_module = create_shader_module(context_.device, *ssao_comp_spv);
    if (!ssao_comp_module) return std::unexpected(ssao_comp_module.error());

    std::vector<VkPipelineShaderStageCreateInfo> pbr_stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, *vert_module, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, *frag_module, "main", nullptr }
    };

    std::vector<VkPipelineShaderStageCreateInfo> skybox_stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, *skybox_vert_module, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, *skybox_frag_module, "main", nullptr }
    };

    std::vector<VkPipelineShaderStageCreateInfo> shadow_stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, *shadow_vert_module, "main", nullptr }
    };

    std::vector<VkPipelineShaderStageCreateInfo> depth_normal_stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, *depth_normal_vert_module, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, *depth_normal_frag_module, "main", nullptr }
    };

    auto toon_vert_spv = read_shader_file("assets/shaders/toon_vert.spv");
    if (!toon_vert_spv) return std::unexpected(toon_vert_spv.error());
    auto toon_frag_spv = read_shader_file("assets/shaders/toon_frag.spv");
    if (!toon_frag_spv) return std::unexpected(toon_frag_spv.error());

    auto toon_vert_module = create_shader_module(context_.device, *toon_vert_spv);
    if (!toon_vert_module) return std::unexpected(toon_vert_module.error());
    auto toon_frag_module = create_shader_module(context_.device, *toon_frag_spv);
    if (!toon_frag_module) return std::unexpected(toon_frag_module.error());

    std::vector<VkPipelineShaderStageCreateInfo> toon_stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, *toon_vert_module, "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, *toon_frag_module, "main", nullptr }
    };

    std::vector<VkPipelineShaderStageCreateInfo> outline_stages = pbr_stages; // アウトラインは頂点シェーダーが違うはずだが今は仮

    // 3. ビューポート設定
    VkViewport const viewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(swapchain_target_.extent.width), .height = static_cast<float>(swapchain_target_.extent.height),
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    VkRect2D const scissor{ .offset = {0, 0}, .extent = swapchain_target_.extent };
    VkPipelineViewportStateCreateInfo const viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1, .pScissors = &scissor
    };

    VkPipelineVertexInputStateCreateInfo const vertex_input_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descriptions.size()),
        .pVertexAttributeDescriptions = attribute_descriptions.data()
    };

    // RenderGraph で MainColorPass の出力先を HDR バッファにしたため、
    // ここでビルドする PBR / Skybox 等のパイプラインも R16G16B16A16_SFLOAT に合わせる
    std::array<VkFormat, 1> color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT };

    // 4. 各パイプラインの作成
    PipelineBuilder builder;
    builder.with_vertex_input(vertex_input_info)
           .with_viewport_state(viewport_state)
           .with_layout(pipeline_layout_);

    // PBR パイプライン (背面カリング)
    auto pbr_res = builder.with_shaders(pbr_stages)
                          .with_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                          .with_depth_test(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS) // 修正: Z-prepassを使わず再描画するため標準のLESSにする
                          .build(context_.device, color_formats, VK_FORMAT_D32_SFLOAT);
    if (!pbr_res) {
        vkDestroyShaderModule(context_.device, *frag_module, nullptr);
        vkDestroyShaderModule(context_.device, *vert_module, nullptr);
        return std::unexpected(pbr_res.error());
    }
    pbr_pipeline_.pipeline = *pbr_res;

    if (config_.material_strategy == MaterialPipelineStrategy::PBR_Toon_Hybrid) {
        // Toon パイプライン (背面カリング)
        auto toon_res = builder.with_shaders(toon_stages)
                               .with_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                               .with_depth_test(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS) // 同上
                               .build(context_.device, color_formats, VK_FORMAT_D32_SFLOAT);
        if (!toon_res) return std::unexpected(toon_res.error()); // TODO: エラーハンドリング整理
        toon_pipeline_.pipeline = *toon_res;

        // Toon Outline パイプライン (表面カリング)
        auto outline_res = builder.with_shaders(outline_stages)
                                  .with_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE)
                                  .with_depth_test(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL) // アウトライン特有の設定
                                  .build(context_.device, color_formats, VK_FORMAT_D32_SFLOAT);
        if (!outline_res) return std::unexpected(outline_res.error());
        toon_outline_pipeline_.pipeline = *outline_res;
    }

    // Skybox パイプライン
    VkPipelineVertexInputStateCreateInfo empty_vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    PipelineBuilder skybox_builder;
    skybox_builder.with_vertex_input(empty_vertex_input)
                  .with_viewport_state(viewport_state)
                  .with_layout(pipeline_layout_)
                  .with_shaders(skybox_stages)
                  .with_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                  .with_depth_test(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL);

    auto skybox_res = skybox_builder.build(context_.device, color_formats, VK_FORMAT_D32_SFLOAT);
    if (!skybox_res) return std::unexpected(skybox_res.error());
    skybox_pipeline_.pipeline = *skybox_res;

    // Shadow パイプライン
    PipelineBuilder shadow_builder;
    // シャドウマップは2048x2048
    VkViewport const shadow_viewport{
        .x = 0.0f, .y = 0.0f,
        .width = 2048.0f, .height = 2048.0f,
        .minDepth = 0.0f, .maxDepth = 1.0f
    };
    VkRect2D const shadow_scissor{ .offset = {0, 0}, .extent = {2048, 2048} };
    VkPipelineViewportStateCreateInfo const shadow_viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &shadow_viewport,
        .scissorCount = 1, .pScissors = &shadow_scissor
    };

    // Shadow pass uses the standard vertex inputs but only writes depth
    shadow_builder.with_vertex_input(vertex_input_info)
                  .with_viewport_state(shadow_viewport_state)
                  .with_layout(pipeline_layout_)
                  .with_shaders(shadow_stages)
                  .with_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                  .with_depth_test(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    std::array<VkFormat, 0> shadow_color_formats = {};
    auto shadow_res = shadow_builder.build(context_.device, shadow_color_formats, VK_FORMAT_D32_SFLOAT);
    if (!shadow_res) return std::unexpected(shadow_res.error());
    shadow_pipeline_.pipeline = *shadow_res;

    // DepthNormal パイプライン
    PipelineBuilder depth_normal_builder;
    depth_normal_builder.with_vertex_input(vertex_input_info)
                        .with_viewport_state(viewport_state)
                        .with_layout(pipeline_layout_)
                        .with_shaders(depth_normal_stages)
                        .with_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
                        .with_depth_test(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);

    std::array<VkFormat, 1> depth_normal_color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    auto depth_normal_res = depth_normal_builder.build(context_.device, depth_normal_color_formats, VK_FORMAT_D32_SFLOAT);
    if (!depth_normal_res) return std::unexpected(depth_normal_res.error());
    depth_normal_pipeline_.pipeline = *depth_normal_res;

    // ToneMap パイプライン (フルスクリーン描画)
    PipelineBuilder tonemap_builder;
    tonemap_builder.with_vertex_input(empty_vertex_input) // 頂点バッファなし
                   .with_viewport_state(viewport_state)
                   .with_layout(pipeline_layout_)
                   .with_shaders(tonemap_stages)
                   .with_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                   .with_depth_test(VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER); // 深度テストなし

    // スワップチェーンに描画するので Swapchain のフォーマット
    std::array<VkFormat, 1> tonemap_color_formats = { swapchain_target_.format };
    auto tonemap_res = tonemap_builder.build(context_.device, tonemap_color_formats, VK_FORMAT_UNDEFINED);
    if (!tonemap_res) return std::unexpected(tonemap_res.error());
    tonemap_pipeline_.pipeline = *tonemap_res;

    // Bloom Extract パイプライン (フルスクリーン描画 -> HDRテクスチャ出力)
    PipelineBuilder bloom_extract_builder;
    bloom_extract_builder.with_vertex_input(empty_vertex_input)
                         .with_viewport_state(viewport_state)
                         .with_layout(pipeline_layout_)
                         .with_shaders(bloom_extract_stages)
                         .with_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                         .with_depth_test(VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER);

    std::array<VkFormat, 1> bloom_extract_color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    auto bloom_extract_res = bloom_extract_builder.build(context_.device, bloom_extract_color_formats, VK_FORMAT_UNDEFINED);
    if (!bloom_extract_res) return std::unexpected(bloom_extract_res.error());
    bloom_extract_pipeline_.pipeline = *bloom_extract_res;

    // Bloom Blur (アナモルフィック・ストリーク) パイプライン (フルスクリーン描画 -> HDRテクスチャ出力)
    PipelineBuilder bloom_blur_builder;
    bloom_blur_builder.with_vertex_input(empty_vertex_input)
                      .with_viewport_state(viewport_state)
                      .with_layout(pipeline_layout_)
                      .with_shaders(bloom_blur_stages)
                      .with_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
                      .with_depth_test(VK_FALSE, VK_FALSE, VK_COMPARE_OP_NEVER);

    std::array<VkFormat, 1> bloom_blur_color_formats = { VK_FORMAT_R16G16B16A16_SFLOAT };
    auto bloom_blur_res = bloom_blur_builder.build(context_.device, bloom_blur_color_formats, VK_FORMAT_UNDEFINED);
    if (!bloom_blur_res) return std::unexpected(bloom_blur_res.error());
    bloom_blur_pipeline_.pipeline = *bloom_blur_res;

    // SSAO コンピュートパイプライン
    auto ssao_comp_res = build_compute_pipeline(context_.device, pipeline_layout_, *ssao_comp_module);
    if (!ssao_comp_res) return std::unexpected(ssao_comp_res.error());
    ssao_pipeline_ = *ssao_comp_res;

    vkDestroyShaderModule(context_.device, *ssao_comp_module, nullptr);
    vkDestroyShaderModule(context_.device, *depth_normal_frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *depth_normal_vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *shadow_vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *skybox_frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *skybox_vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *tonemap_frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *tonemap_vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *bloom_extract_frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *bloom_extract_vert_module, nullptr);
    vkDestroyShaderModule(context_.device, *bloom_blur_frag_module, nullptr);
    vkDestroyShaderModule(context_.device, *bloom_blur_vert_module, nullptr);

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

std::expected<void, EngineError> VulkanRenderer::initialize_imgui() {
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    if (vkCreateDescriptorPool(context_.device, &pool_info, nullptr, &imgui_pool_) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"ImGui Descriptor Pool creation failed"}});
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(config_.window_handle), true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = context_.instance;
    init_info.PhysicalDevice = context_.physical_device;
    init_info.Device = context_.device;
    init_info.QueueFamily = context_.graphics_queue_family_index;
    init_info.Queue = context_.graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_pool_;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;

    init_info.UseDynamicRendering = true;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain_target_.format
    };

    ImGui_ImplVulkan_Init(&init_info);

    return {};
}

}  // namespace vanta::render

