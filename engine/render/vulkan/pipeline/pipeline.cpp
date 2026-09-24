//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include "pipeline.h"

#include <fstream>

namespace vanta::render {

namespace {
    [[nodiscard]] std::expected<std::vector<char>, EngineError> read_file(const std::string& filename) noexcept {
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

    [[nodiscard]] std::expected<VkShaderModule, EngineError> create_shader_module(
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
} // namespace

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

    std::expected<GraphicsPipeline, EngineError> GraphicsPipeline::create(
    VkDevice device,
    VkFormat color_attachment_format,
    VkFormat depth_attachment_format,
    VkExtent2D extent,
    std::span<const VkDescriptorSetLayout> descriptor_set_layouts,
    const VkVertexInputBindingDescription& binding_desc,
    std::span<const VkVertexInputAttributeDescription> attrib_desc
) noexcept {

    // 1. シェーダーの読み込みとモジュール生成
    auto vert_spv = read_file("assets/shaders/main_vert.spv");
    if (!vert_spv) { return std::unexpected(vert_spv.error()); }

    auto frag_spv = read_file("assets/shaders/main_frag.spv");
    if (!frag_spv) { return std::unexpected(frag_spv.error()); }

    auto vert_module = create_shader_module(device, vert_spv.value());
    if (!vert_module) { return std::unexpected(vert_module.error()); }

    auto frag_module = create_shader_module(device, frag_spv.value());
    if (!frag_module) {
        vkDestroyShaderModule(device, vert_module.value(), nullptr);
        return std::unexpected(frag_module.error());
    }

    std::vector<VkPipelineShaderStageCreateInfo> shader_stages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_module.value(),
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_module.value(),
            .pName = "main"
        }
    };

    VkPipelineVertexInputStateCreateInfo const vertex_input{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_desc,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attrib_desc.size()),
        .pVertexAttributeDescriptions = attrib_desc.data()
    };

    std::array viewports = { VkViewport{
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(extent.width), .height = static_cast<float>(extent.height),
        .minDepth = 0.0f, .maxDepth = 1.0f
    }};

    std::array scissors = { VkRect2D{
        .offset = {0, 0}, .extent = extent
    }};

    VkPipelineViewportStateCreateInfo const viewport_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = viewports.data(),
        .scissorCount = 1,
        .pScissors = scissors.data()
    };

    VkPipelineLayoutCreateInfo const layout_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    VkPipelineLayout layout = nullptr;
    if (vkCreatePipelineLayout(device, &layout_info, nullptr, &layout) != VK_SUCCESS) {
        vkDestroyShaderModule(device, vert_module.value(), nullptr);
        vkDestroyShaderModule(device, frag_module.value(), nullptr);
        return std::unexpected(EngineError{LegacyError{"PipelineLayout生成失敗"}});
    }

    PipelineBuilder builder;
    std::array color_formats = { color_attachment_format };

    auto pipeline_result = builder
        .with_shaders(std::move(shader_stages))
        .with_vertex_input(vertex_input)
        .with_viewport_state(viewport_state)
        .with_layout(layout)
        .build(device, color_formats, depth_attachment_format);

    vkDestroyShaderModule(device, vert_module.value(), nullptr);
    vkDestroyShaderModule(device, frag_module.value(), nullptr);

    if (!pipeline_result) {
        vkDestroyPipelineLayout(device, layout, nullptr);
        return std::unexpected(pipeline_result.error());
    }

    return GraphicsPipeline{ layout, pipeline_result.value() };
}

void GraphicsPipeline::destroy(VkDevice device) const noexcept {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, pipeline, nullptr);
    }
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, layout, nullptr);
    }
}

}  // namespace vanta::render

