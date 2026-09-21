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
#include "include/render_types.h"
#include "vulkan/resources/buffers/vulkan_buffer_utils.h"
#include "vulkan/commands/command_recorder.h"
#include "vulkan/resources/descriptors/descriptor.h"
#include "vulkan/resources/images/texture.h"
#include "vulkan/pipeline/pipeline.h"
#include "vulkan//render/swapchain_target.h"
#include "vulkan/frame/frame_context.h"

namespace vanta::render {

    struct ActiveFrame {
        CommandRecorder recorder;
        uint32_t image_index{0};
        uint32_t frame_index{0};
    };

    /// GPUに転送済みのメッシュデータ
    struct GpuMesh {
        GpuBuffer vertex_buffer;
        GpuBuffer index_buffer;
        uint32_t index_count{};
    };

    class VulkanRenderer {
    public:
        [[nodiscard]] static std::expected<VulkanRenderer, EngineError> create(
            const char* app_name,
            void* window_handle,
            uint32_t window_width,
            uint32_t window_height);


        ~VulkanRenderer();

        VulkanRenderer(VulkanRenderer&& other) noexcept;
        VulkanRenderer& operator=(VulkanRenderer&& other) noexcept;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        // メインAPI
        [[nodiscard]] std::expected<MeshId, EngineError> create_mesh_from_data(const MeshData& data);
        [[nodiscard]] std::expected<void, EngineError> draw_frame(const RenderSnapshot& snapshot);
        std::expected<void, std::string> load_scene(const std::string& filepath);

    private:
        VulkanRenderer() = default;

        [[nodiscard]] std::expected<ActiveFrame, EngineError> begin_frame() const;
        [[nodiscard]] std::expected<void, EngineError> end_frame(const ActiveFrame& active_frame);
        [[nodiscard]] std::expected<void, EngineError> initialize_descriptor_resources();
        [[nodiscard]] std::expected<void, EngineError> initialize_pipeline_resources();
        [[nodiscard]] static GlobalUbo build_global_ubo(const RenderSnapshot& snapshot);

        // --- サブシステム群 ---
        VulkanContext context_;
        SwapchainTarget swapchain_target_;
        GraphicsPipeline pipeline_;
        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frames_;
        uint32_t current_frame_index_{0};

        std::vector<GpuMesh> meshes_;

        // --- Descriptor 関連 ---
        GpuBuffer global_ubo_buffer_;
        VkDescriptorPool descriptor_pool_{VK_NULL_HANDLE};
        VkDescriptorSetLayout descriptor_set_layout_{VK_NULL_HANDLE};
        VkDescriptorSet global_descriptor_set_{VK_NULL_HANDLE};

        // UBO用 (Set 0)
        VkDescriptorSetLayout ubo_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool ubo_pool_ = VK_NULL_HANDLE;
        VkDescriptorSet global_ubo_set_ = VK_NULL_HANDLE;

        // Bindless用 (Set 1)
        VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool bindless_pool_ = VK_NULL_HANDLE;
        VkDescriptorSet global_bindless_set_ = VK_NULL_HANDLE;

        std::expected<void, EngineError> initialize_textures();
        std::vector<vanta::vulkan::Texture> textures_;
        [[nodiscard]] FrameContext& current_frame() noexcept { return frames_[current_frame_index_]; }

        AllocatedBuffer vertex_buffer_;
        AllocatedBuffer index_buffer_;
        uint32_t index_count_ = 0;
    };

}  // namespace vanta::render

