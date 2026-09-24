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

    // パイプライン生成
    [[nodiscard]] std::expected<VkPipeline, EngineError> build(
        VkDevice device,
        std::span<const VkFormat> color_formats,
        VkFormat depth_format = VK_FORMAT_UNDEFINED,
        VkFormat stencil_format = VK_FORMAT_UNDEFINED
    ) const noexcept;
};

struct GraphicsPipeline {
    VkPipelineLayout layout{VK_NULL_HANDLE};
    VkPipeline pipeline{VK_NULL_HANDLE};

    // 静的ファクトリ関数
    [[nodiscard]] static std::expected<GraphicsPipeline, EngineError> create(
    VkDevice device,
    VkFormat color_attachment_format,
    VkFormat depth_attachment_format,
    VkExtent2D extent,
    std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
    const VkVertexInputBindingDescription& binding_desc,
    std::span<const VkVertexInputAttributeDescription> attrib_desc
) noexcept;

    void destroy(VkDevice device) const noexcept;
};

}  // namespace vanta::render
