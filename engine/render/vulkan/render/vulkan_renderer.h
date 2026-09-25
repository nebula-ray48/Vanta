//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <vector>
#include <array>

#include "engine_error.h"
#include "render_types.h"
#include "vulkan/resources/buffers/vulkan_buffer_utils.h"
#include "vulkan/commands/command_recorder.h"
#include "vulkan/resources/descriptors/descriptor.h"
#include "vulkan/resources/images/texture.h"
#include "vulkan/pipeline/pipeline.h"
#include "vulkan//render/swapchain_target.h"
#include "vulkan/frame/frame_context.h"
#include "vulkan/resources/resource_registry.h"

namespace vanta::render {

    struct ActiveFrame {
        CommandRecorder recorder;
        uint32_t image_index{0};
        uint32_t frame_index{0};
    };

    /// GPUに転送済みのメッシュデータ
    struct GpuMesh {
        uint32_t first_index;   // グローバルバッファ内のインデックス開始位置
        uint32_t index_count;   // インデックス数
        int32_t  vertex_offset; // グローバルバッファ内の頂点開始位置
    };


    struct alignas(16) GpuObjectData {
        glm::mat4 model_matrix;
        uint32_t data[16]; // 汎用ペイロード (64 bytes)
    };

    /**
     * @class VulkanRenderer
     * @brief Vantaエンジンのメインレンダラークラス
     *
     * Vulkanの初期化、リソース管理、描画パスの構築と実行を担当します。
     * 内部は用途ごとに以下の cpp ファイルに分割して実装されています：
     * - vulkan_renderer_core.cpp : コンテキストの初期化、破棄、リソース移動管理
     * - vulkan_renderer_init.cpp : デスクリプタ、パイプライン、バッファなどの初期化ロジック
     * - vulkan_renderer_draw.cpp : フレームごとの描画ループ、Render Graphの構築と実行
     */
    class VulkanRenderer {
    public:
        [[nodiscard]] static std::expected<VulkanRenderer, EngineError> create(
            const RendererConfig& config);


        ~VulkanRenderer();

        VulkanRenderer(VulkanRenderer&& other) noexcept;
        VulkanRenderer& operator=(VulkanRenderer&& other) noexcept;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        // メインAPI
        struct LoadedSceneNode {
            MeshId mesh_id;
            MaterialData material;
            glm::mat4 global_transform{1.0f};
        };

        [[nodiscard]] std::expected<MeshId, EngineError> create_mesh_from_data(const MeshData& data);
        [[nodiscard]] std::expected<void, EngineError> draw_frame(const RenderSnapshot& snapshot);
        [[nodiscard]] std::expected<void, EngineError> resize(uint32_t width, uint32_t height);
        [[nodiscard]] std::expected<std::vector<LoadedSceneNode>, std::string> load_scene(const std::string& filepath);
        [[nodiscard]] std::expected<uint32_t, EngineError> register_texture(vanta::vulkan::Texture&& texture);

        // ImGui helpers for test application
        void begin_imgui_frame();
        void end_imgui_frame(VkCommandBuffer cmd);

        [[nodiscard]] PostProcessSettings& post_process_settings() { return post_process_settings_; }
        [[nodiscard]] const PostProcessSettings& post_process_settings() const { return post_process_settings_; }

    private:
        VulkanRenderer() = default;

        [[nodiscard]] std::expected<ActiveFrame, EngineError> begin_frame() const;
        [[nodiscard]] std::expected<void, EngineError> end_frame(const ActiveFrame& active_frame);
        [[nodiscard]] std::expected<void, EngineError> initialize_descriptor_resources();
        [[nodiscard]] std::expected<void, EngineError> initialize_pipeline_resources();
        [[nodiscard]] GlobalUbo build_global_ubo(const RenderSnapshot& snapshot) const;

        // --- サブシステム群 ---
        RendererConfig config_;
        VulkanContext context_;
        SwapchainTarget swapchain_target_;
        PostProcessSettings post_process_settings_{};

        // パイプライン群 (全て同じPipelineLayoutを共有)
        VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
        GraphicsPipeline pbr_pipeline_;
        GraphicsPipeline toon_pipeline_;
        GraphicsPipeline toon_outline_pipeline_;
        GraphicsPipeline skybox_pipeline_;
        GraphicsPipeline shadow_pipeline_;
        GraphicsPipeline depth_normal_pipeline_;
        GraphicsPipeline tonemap_pipeline_;
        GraphicsPipeline bloom_extract_pipeline_;
        GraphicsPipeline bloom_blur_pipeline_;
        ComputePipeline ssao_pipeline_;

        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frames_;
        uint32_t current_frame_index_{0};

        std::vector<GpuMesh> meshes_;

        // --- Descriptor 関連 ---
        GpuBuffer global_ubo_buffer_;

        // Bindless用 (Set 0)
        VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool bindless_pool_ = VK_NULL_HANDLE;
        VkDescriptorSet global_bindless_set_ = VK_NULL_HANDLE;

        // SSAO Specific Descriptor (Set 1)
        VkDescriptorSetLayout ssao_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool ssao_pool_ = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ssao_sets_{VK_NULL_HANDLE};

        // ImGui
        VkDescriptorPool imgui_pool_ = VK_NULL_HANDLE;
        [[nodiscard]] std::expected<void, EngineError> initialize_imgui();

        std::expected<void, EngineError> initialize_textures();
        std::vector<vanta::vulkan::Texture> textures_;
        std::optional<vanta::vulkan::Texture> env_cubemap_;
        std::optional<vanta::vulkan::Texture> ssao_noise_tex_;
        std::array<glm::vec4, 64> ssao_samples_{};
        uint32_t brdf_lut_index_ = 0;

        uint32_t shadow_map_index_ = 0;

        [[nodiscard]] FrameContext& current_frame() noexcept { return frames_[current_frame_index_]; }

        ResourceRegistry registry_;
        std::vector<ImageHandle> swapchain_image_handles_;
        ImageHandle shadow_map_handle_;

        GpuBuffer global_vertex_buffer_;
        GpuBuffer global_index_buffer_;
        uint32_t global_vertex_count_ = 0;
        uint32_t global_index_count_ = 0;
        uint32_t index_count_ = 0;

        GpuBuffer object_buffer_;
        GpuBuffer indirect_buffer_;
        [[nodiscard]] std::expected<void, EngineError> initialize_draw_buffers();
    };

}  // namespace vanta::render

