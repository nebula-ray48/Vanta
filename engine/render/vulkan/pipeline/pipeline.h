//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <span>
#include <vector>

#include "engine_error.h"

namespace vanta::render {

    /**
     * @class PipelineBuilder
     * @brief Vulkanのグラフィックスパイプラインを段階的に構築するためのビルダー
     * 
     * 複雑な `VkGraphicsPipelineCreateInfo` の設定を抽象化し、メソッドチェーンで
     * 必要な状態（シェーダー、頂点入力、ビューポートなど）を設定できるようにします。
     * Vulkan 1.3 の Dynamic Rendering (`VK_KHR_dynamic_rendering`) に対応しており、
     * RenderPass オブジェクトなしでパイプラインを生成できます。
     */
    class PipelineBuilder {
private:
    std::vector<VkPipelineShaderStageCreateInfo> shader_stages_;
    VkPipelineVertexInputStateCreateInfo vertex_input_info_{};
    VkPipelineInputAssemblyStateCreateInfo input_assembly_{};
    VkPipelineViewportStateCreateInfo viewport_state_{};
    VkPipelineRasterizationStateCreateInfo rasterizer_{};
    VkPipelineMultisampleStateCreateInfo multisampling_{};
    VkPipelineColorBlendAttachmentState color_blend_attachment_{};
    VkPipelineDepthStencilStateCreateInfo depth_stencil_{};
    VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};

public:
    PipelineBuilder() noexcept;

    PipelineBuilder& with_shaders(std::vector<VkPipelineShaderStageCreateInfo> stages) noexcept {
        shader_stages_ = std::move(stages);
        return *this;
    }

    PipelineBuilder& with_vertex_input(const VkPipelineVertexInputStateCreateInfo& info) noexcept {
        vertex_input_info_ = info;
        return *this;
    }

    PipelineBuilder& with_viewport_state(const VkPipelineViewportStateCreateInfo& info) noexcept {
        viewport_state_ = info;
        return *this;
    }

    PipelineBuilder& with_layout(VkPipelineLayout layout) noexcept {
        pipeline_layout_ = layout;
        return *this;
    }

    PipelineBuilder& with_cull_mode(VkCullModeFlags cull_mode, VkFrontFace front_face) noexcept {
        rasterizer_.cullMode = cull_mode;
        rasterizer_.frontFace = front_face;
        return *this;
    }

    PipelineBuilder& with_depth_test(VkBool32 depth_test_enable, VkBool32 depth_write_enable, VkCompareOp compare_op) noexcept {
        depth_stencil_.depthTestEnable = depth_test_enable;
        depth_stencil_.depthWriteEnable = depth_write_enable;
        depth_stencil_.depthCompareOp = compare_op;
        return *this;
    }

    PipelineBuilder& with_color_blend(const VkPipelineColorBlendAttachmentState& blend) noexcept {
        color_blend_attachment_ = blend;
        return *this;
    }

    // パイプライン生成
    [[nodiscard]] std::expected<VkPipeline, EngineError> build(
        VkDevice device,
        std::span<const VkFormat> color_formats,
        VkFormat depth_format = VK_FORMAT_UNDEFINED,
        VkFormat stencil_format = VK_FORMAT_UNDEFINED
    ) const noexcept;
};

// ユーティリティ
[[nodiscard]] std::expected<std::vector<char>, EngineError> read_shader_file(const std::string& filename) noexcept;
[[nodiscard]] std::expected<VkShaderModule, EngineError> create_shader_module(VkDevice device, std::span<const char> code) noexcept;

struct GraphicsPipeline {
    VkPipeline pipeline{VK_NULL_HANDLE};

    void destroy(VkDevice device) const noexcept;
};

}  // namespace vanta::render
