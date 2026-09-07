#include "graph_compiler.h"
#include "engine_error.h"
#include <unordered_map>

namespace vanta::render::fg {

inline void translate_usage_to_vulkan(
    UsageType usage,
    VkPipelineStageFlags2& stage,
    VkAccessFlags2& access,
    VkImageLayout& layout
) noexcept {
    switch (usage) {
        case UsageType::READ_TEXTURE:
            stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT;
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;

        case UsageType::WRITE_DEPTH:
            stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            break;

        case UsageType::WRITE_COLOR:
            stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;

        case UsageType::PRESENT:
            stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            access = VK_ACCESS_2_NONE;
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;

        case UsageType::COMPUTE_READ:
        case UsageType::COMPUTE_WRITE:
            stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            access = (usage == UsageType::COMPUTE_READ) ? VK_ACCESS_2_SHADER_READ_BIT : VK_ACCESS_2_SHADER_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_GENERAL;
            break;

        case UsageType::TRANSFER_SRC:
        case UsageType::TRANSFER_DST:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            access = (usage == UsageType::TRANSFER_SRC) ? VK_ACCESS_2_TRANSFER_READ_BIT : VK_ACCESS_2_TRANSFER_WRITE_BIT;
            layout = (usage == UsageType::TRANSFER_SRC) ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;

        default:
            stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            access = VK_ACCESS_2_NONE;
            layout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
    }
}

[[nodiscard]] std::expected<ExecutionPlan, EngineError> compile_graph(
    const RenderGraphData& graph_data
) noexcept {
    ExecutionPlan plan{};

    plan.sorted_passes = graph_data.passes;

    std::unordered_map<uint32_t, UsageType> resource_states;

    for (const auto& pass : plan.sorted_passes) {

        for (uint32_t i = 0; i < pass.read_images_count; ++i) {

            const PassResource& resource = graph_data.all_read_images[pass.read_images_offset + i];

            uint32_t image_id = resource.handle.id;
            UsageType new_usage = resource.usage;

            UsageType old_usage = static_cast<UsageType>(255);
            if (resource_states.contains(image_id)) {
                old_usage = resource_states[image_id];
            }

            if (old_usage != new_usage) {
                VkPipelineStageFlags2 old_stage, new_stage;
                VkAccessFlags2 old_access, new_access;
                VkImageLayout old_layout, new_layout;

                translate_usage_to_vulkan(old_usage, old_stage, old_access, old_layout);
                translate_usage_to_vulkan(new_usage, new_stage, new_access, new_layout);

                VkImageMemoryBarrier2 barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

                // TODO ※重要: コンパイル時点ではまだ本物の VkImage は分からないので、
                // 実行時に image_id から本物の VkImage を引っ張ってくる仕組みにする。
                // barrier.image = ...;

                // Before
                barrier.oldLayout = old_layout;
                barrier.srcStageMask = old_stage;
                barrier.srcAccessMask = old_access;
                // After
                barrier.newLayout = new_layout;
                barrier.dstStageMask = new_stage;
                barrier.dstAccessMask = new_access;

                // TODO: 完成した barrier を ExecutionPlan に保存する
            }

            resource_states[image_id] = new_usage;
        }

        for (uint32_t i = 0; i < pass.write_images_count; ++i) {
             const PassResource& resource = graph_data.all_write_images[pass.write_images_offset + i];
        }
    }

    return plan;
}

}  // namespace vanta::render::fg
