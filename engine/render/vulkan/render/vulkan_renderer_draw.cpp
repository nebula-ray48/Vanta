//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#include <array>

#include "vulkan/resources/buffers/buffer.h"
#include "vulkan/frame_graph/graph_builder.h"
#include "vulkan/frame_graph/graph_compiler.h"
#include "vulkan/frame_graph/graph_executor.h"
#include "vulkan/frame_graph/pass_context.h"
#include "vulkan/resources/resource_registry.h"
#include "vulkan_renderer.h"

namespace vanta::render {

GlobalUbo VulkanRenderer::build_global_ubo(const RenderSnapshot& snapshot) {
    return GlobalUbo{
        .view_proj = snapshot.view_matrix,
        .camera_pos = glm::vec3(0.0f),
        .padding = 0.0f,
    };
}

std::expected<void, EngineError> VulkanRenderer::draw_frame(const RenderSnapshot& snapshot) {
    auto active_frame_opt = begin_frame();
    if (!active_frame_opt) {
        return std::unexpected(active_frame_opt.error());
    }

    ActiveFrame active_frame = *active_frame_opt;

    // ---------------------------------------------------------
    // Phase 1: CPU->GPU バッファの更新 (Global UBO)
    // ---------------------------------------------------------
    if (auto res = update_buffer_data(context_, global_ubo_buffer_, build_global_ubo(snapshot)); !res) {
        return std::unexpected(res.error());
    }

    // ---------------------------------------------------------
    // Phase 2: CPU->GPU バッファの更新 (Object Data & Indirect Command)
    // - vmaMapMemoryでメモリをCPU側から見えるようにマップする
    // - エンティティ(Instance)の数だけループしてデータを書き込む
    // - vmaUnmapMemoryで変更を確定させる
    // ---------------------------------------------------------
    GpuObjectData* object_data = nullptr;
    VkDrawIndexedIndirectCommand* indirect_cmds = nullptr;
    vmaMapMemory(context_.allocator, object_buffer_.allocation, (void**)&object_data);
    vmaMapMemory(context_.allocator, indirect_buffer_.allocation, (void**)&indirect_cmds);

    uint32_t instance_idx = 0;
    for (const auto& instance : snapshot.instances) {
        object_data[instance_idx].model_matrix = instance.model_matrix;

        if (instance.mesh_id.value < meshes_.size()) {
            const auto& mesh = meshes_[instance.mesh_id.value];
            indirect_cmds[instance_idx].indexCount = mesh.index_count;
            indirect_cmds[instance_idx].instanceCount = 1;
            indirect_cmds[instance_idx].firstIndex = mesh.first_index;
            indirect_cmds[instance_idx].vertexOffset = mesh.vertex_offset;
            indirect_cmds[instance_idx].firstInstance = instance_idx;
        }
        instance_idx++;
    }

    vmaUnmapMemory(context_.allocator, object_buffer_.allocation);
    vmaUnmapMemory(context_.allocator, indirect_buffer_.allocation);

    // ---------------------------------------------------------
    // Phase 3: Render Graph の構築
    // - Virtual Resource の宣言と Pass の登録を行う
    // ---------------------------------------------------------
    fg::RenderGraphBuilder graph_builder;
    const fg::ImageHandle swapchain_image = swapchain_image_handles_[active_frame.image_index];
    graph_builder.import_image(
        swapchain_image,
        fg::ImageDescription{
            .width = swapchain_target_.extent.width,
            .height = swapchain_target_.extent.height,
            .format = swapchain_target_.format,
        },
        fg::UsageType::Undefined);

    const fg::ImageHandle depth_image = graph_builder.create_image(
        fg::ImageDescription{
            .width = swapchain_target_.extent.width,
            .height = swapchain_target_.extent.height,
            .format = VK_FORMAT_D32_SFLOAT,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        });

    graph_builder.add_pass("MainColorPass")
        .write_image(swapchain_image, fg::UsageType::ColorAttachment)
        .write_image(depth_image, fg::UsageType::DepthAttachment)
        .execute([this, &snapshot, swapchain_image, depth_image](const fg::PassContext& ctx) {
            VkCommandBuffer cmd = ctx.command_buffer();
            VkRenderingAttachmentInfo color_attachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = ctx.get_image_view(swapchain_image),
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{0.0f, 0.0f, 1.0f, 1.0f}}},
            };
            VkRenderingAttachmentInfo depth_attachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = ctx.get_image_view(depth_image),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .clearValue = {{{1.0f, 0}},},
            };
            const VkRenderingInfo rendering_info{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.offset = {0, 0}, .extent = swapchain_target_.extent},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &color_attachment,
                .pDepthAttachment = &depth_attachment,
            };

            vkCmdBeginRendering(cmd, &rendering_info);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_.pipeline);

            const VkViewport viewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(swapchain_target_.extent.width),
                .height = static_cast<float>(swapchain_target_.extent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            const VkRect2D scissor{
                .offset = {0, 0},
                .extent = swapchain_target_.extent,
            };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            std::array<VkDescriptorSet, 1> bound_sets = {
                global_bindless_set_,
            };
            vkCmdBindDescriptorSets(
                cmd,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_.layout,
                0,
                static_cast<uint32_t>(bound_sets.size()),
                bound_sets.data(),
                0,
                nullptr);

            VkBuffer vertex_buffers[] = {global_vertex_buffer_.buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(cmd, global_index_buffer_.buffer, 0, VK_INDEX_TYPE_UINT32);

            if (!snapshot.instances.empty()) {
                vkCmdDrawIndexedIndirect(
                    cmd, 
                    indirect_buffer_.buffer, 
                    0, 
                    static_cast<uint32_t>(snapshot.instances.size()),
                    sizeof(VkDrawIndexedIndirectCommand)
                );
            }

            vkCmdEndRendering(cmd);
        });

    // ---------------------------------------------------------
    // Phase 4: Render Graph のコンパイルと実行
    // - コンパイラが依存関係を解析し、バリアと実行順序を決定 (plan)
    // - GraphExecutor が plan に従って実際の Vulkan API を呼び出す
    // ---------------------------------------------------------

    graph_builder.add_pass("PresentPass")
        .read_image(swapchain_image, fg::UsageType::Present);

    const fg::RenderGraphData graph_data = graph_builder.build();
    const auto plan = fg::compile_graph(graph_data);
    if (!plan) {
        return std::unexpected(EngineError{LegacyError(to_string(plan.error()))});
    }

    fg::GraphExecutor executor;
    const auto execute_result = executor.execute(
        active_frame.recorder.command_buffer, *plan, graph_data, registry_, context_);
    if (!execute_result) {
        return std::unexpected(EngineError{LegacyError(execute_result.error())});
    }

    return end_frame(active_frame);
}

std::expected<ActiveFrame, EngineError> VulkanRenderer::begin_frame() const {
    const uint32_t frame = current_frame_index_;
    const FrameContext& frame_context = frames_[frame];

    if (vkWaitForFences(context_.device, 1, &frame_context.completion_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"Fenceの待機に失敗"}});
    }

    uint32_t image_index = 0;
    const VkResult result = vkAcquireNextImageKHR(
        context_.device,
        swapchain_target_.swapchain,
        UINT64_MAX,
        frame_context.image_available,
        VK_NULL_HANDLE,
        &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return std::unexpected(EngineError{LegacyError{"画像の取得に失敗"}});
    }

    vkResetFences(context_.device, 1, &frame_context.completion_fence);

    VkCommandBuffer cmd = frame_context.graphics_command_buffer;
    vkResetCommandBuffer(cmd, 0);

    constexpr VkCommandBufferBeginInfo begin_info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"コマンドバッファの記録開始に失敗"}});
    }

    return ActiveFrame{
        .recorder = CommandRecorder{context_.device, cmd},
        .image_index = image_index,
        .frame_index = frame,
    };
}

std::expected<void, EngineError> VulkanRenderer::end_frame(const ActiveFrame& active_frame) {
    if (vkEndCommandBuffer(active_frame.recorder.command_buffer) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"コマンドバッファの終了に失敗"}});
    }

    const FrameContext& frame_context = frames_[active_frame.frame_index];
    VkSemaphore wait_semaphores[] = {frame_context.image_available};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkCommandBuffer command_buffers[] = {active_frame.recorder.command_buffer};
    VkSemaphore signal_semaphores[] = {
        swapchain_target_.render_finished_semaphores[active_frame.image_index]
    };

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = command_buffers,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signal_semaphores,
    };

    if (vkQueueSubmit(context_.graphics_queue, 1, &submit_info, frame_context.completion_fence) != VK_SUCCESS) {
        return std::unexpected(EngineError{LegacyError{"キューの送信に失敗"}});
    }

    const VkSwapchainKHR swapchains[] = {swapchain_target_.swapchain};
    const VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &active_frame.image_index,
    };

    vkQueuePresentKHR(context_.graphics_queue, &present_info);

    current_frame_index_ = (active_frame.frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
    return {};
}

}  // namespace vanta::render

