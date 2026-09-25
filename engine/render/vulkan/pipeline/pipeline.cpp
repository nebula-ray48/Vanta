//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "pipeline.h"

#include <fstream>

namespace vanta::render {

std::expected<std::vector<char>, EngineError> read_shader_file(const std::string& filename) noexcept {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(LegacyError("ファイルの読み込みに失敗しました: " + filename));
    }

    size_t const file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));
    file.close();

    return buffer;
}

std::expected<VkShaderModule, EngineError> create_shader_module(
    VkDevice device,
    std::span<const char> code
) noexcept {
    VkShaderModuleCreateInfo const create_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule shader_module;
    if (vkCreateShaderModule(device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
        return std::unexpected(LegacyError("シェーダーモジュールの生成に失敗しました"));
    }
    return shader_module;
}

PipelineBuilder::PipelineBuilder() noexcept {
    input_assembly_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    rasterizer_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE, // 両面描画
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    multisampling_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    color_blend_attachment_ = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                            VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT
    };

    depth_stencil_ = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE
    };
}

std::expected<VkPipeline, EngineError> PipelineBuilder::build(
    VkDevice device,
    std::span<const VkFormat> color_formats,
    VkFormat depth_format,
    VkFormat stencil_format
) const noexcept {
    if (pipeline_layout_ == VK_NULL_HANDLE) {
        return std::unexpected(EngineError{LegacyError{"Pipeline Layout が設定されていません"}});
    }

    VkPipelineColorBlendStateCreateInfo const color_blend_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment_
    };

    constexpr std::array dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo const dynamic_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data()
    };

    // =================================================================
    // 注目: Dynamic Rendering 用の構造体
    // =================================================================
    VkPipelineRenderingCreateInfo const rendering_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(color_formats.size()),
        .pColorAttachmentFormats = color_formats.data(),
        .depthAttachmentFormat = depth_format,
        .stencilAttachmentFormat = stencil_format
    };

    VkGraphicsPipelineCreateInfo const pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .stageCount = static_cast<uint32_t>(shader_stages_.size()),
        .pStages = shader_stages_.data(),
        .pVertexInputState = &vertex_input_info_,
        .pInputAssemblyState = &input_assembly_,
        .pViewportState = &viewport_state_,
        .pRasterizationState = &rasterizer_,
        .pMultisampleState = &multisampling_,
        .pDepthStencilState = &depth_stencil_,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_layout_,
        .renderPass = VK_NULL_HANDLE,
        .subpass = 0
    };

    VkPipeline pipeline = nullptr;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"GraphicsPipeline生成失敗"}});
    }

    return pipeline;
}


void GraphicsPipeline::destroy(VkDevice device) const noexcept {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}

void ComputePipeline::destroy(VkDevice device) const noexcept {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
}

std::expected<ComputePipeline, EngineError> build_compute_pipeline(
    VkDevice device,
    VkPipelineLayout layout,
    VkShaderModule compute_shader) noexcept {
    
    VkPipelineShaderStageCreateInfo stage_info{};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = compute_shader;
    stage_info.pName = "main";
    
    VkComputePipelineCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    create_info.layout = layout;
    create_info.stage = stage_info;
    
    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"コンピュートパイプラインの作成に失敗しました"}});
    }
    
    return ComputePipeline{pipeline};
}

}  // namespace vanta::render

