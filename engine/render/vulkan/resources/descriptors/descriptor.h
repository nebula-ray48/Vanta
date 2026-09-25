//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <expected>

#include "engine_error.h"
#include "vulkan/core/vulkan_context.h"

namespace vanta::render {

    struct alignas(16) GlobalUbo {
        glm::mat4 view_proj;
        glm::mat4 inv_view_proj;
        glm::mat4 light_view_proj;
        glm::vec3 camera_pos;
        float padding;
        glm::vec4 sun_direction;
        glm::vec4 sun_color;
        glm::vec4 ambient_color;
        glm::vec4 sh[9];
        uint32_t brdf_lut_index;
        float max_reflection_lod;
        uint32_t shadow_map_index;
        uint32_t ssao_map_index;
        glm::mat4 view_matrix;
        
        // SSAO Params
        glm::mat4 proj_matrix;
        glm::mat4 inv_proj_matrix;
        glm::vec4 ssao_samples[64];
        glm::vec2 screen_size;
        float ssao_radius;
        float ssao_bias;
    };

    /**
     * @class BindlessDescriptorLayout
     * @brief Bindlessアーキテクチャのための共通 Descriptor Set 0 のレイアウトを定義します。
     * 
     * 全てのシェーダーで共通して使用されるグローバルなリソースをバインドするためのレイアウトです。
     * 現在のバインディング設計：
     * - Binding 0 (Uniform Buffer)         : GlobalUbo (カメラ情報、SH係数など)
     * - Binding 1 (Sampled Image)          : 2Dテクスチャの配列（UpdateAfterBind対応）
     * - Binding 2 (Sampler)                : 共通サンプラー
     * - Binding 3 (Storage Buffer)         : 全オブジェクトの GpuObjectData を格納するSSBO
     * - Binding 4 (Combined Image Sampler) : IBL キューブマップ (SamplerCube)
     */
    class BindlessDescriptorLayout {
    public:
        [[nodiscard]] static std::expected<VkDescriptorSetLayout, EngineError> create(VkDevice device) noexcept;

        static void destroy(VkDevice device, VkDescriptorSetLayout layout) noexcept;
    };

    /**
     * @class BindlessDescriptorManager
     * @brief Bindless用の巨大なDescriptor Poolの管理と、Setの割り当て・更新を行います。
     */
    class BindlessDescriptorManager {
    public:
        [[nodiscard]] static std::expected<VkDescriptorPool, EngineError> create_pool(VkDevice device) noexcept;

        static void destroy_pool(VkDevice device, VkDescriptorPool pool) noexcept;

        [[nodiscard]] static std::expected<VkDescriptorSet, EngineError> allocate_set(
            VkDevice device,
            VkDescriptorPool pool,
            VkDescriptorSetLayout layout) noexcept;

        static void update_ubo(
            VkDevice device,
            VkDescriptorSet set,
            VkBuffer ubo_buffer,
            size_t ubo_size) noexcept;

        static void update_cubemap(
            VkDevice device,
            VkDescriptorSet set,
            VkImageView cubemap_view,
            VkSampler cubemap_sampler) noexcept;
    };

}  // namespace vanta::render

