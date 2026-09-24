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

#include <glm/gtc/matrix_transform.hpp>

namespace vanta::render {

GlobalUbo VulkanRenderer::build_global_ubo(const RenderSnapshot& snapshot) const {
    glm::vec3 sun_dir = glm::normalize(snapshot.sun_direction);
    glm::mat4 light_proj = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, -50.0f, 50.0f);
    light_proj[1][1] *= -1.0f;
    glm::mat4 light_view = glm::lookAt(sun_dir * 20.0f, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    GlobalUbo ubo{
        .view_proj = snapshot.view_matrix,
        .inv_view_proj = glm::inverse(snapshot.view_matrix),
        .light_view_proj = light_proj * light_view,
        .camera_pos = snapshot.camera_pos,
        .padding = 0.0f,
        .sun_direction = glm::vec4(sun_dir, 3.0f), 
        .sun_color = glm::vec4(1.0f, 1.0f, 0.95f, 1.0f),
        .ambient_color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f),
        .sh = {
            glm::vec4( 1.389567f,  1.609414f,  1.902710f, 0.0f),
            glm::vec4( 0.036784f,  0.039627f,  0.043386f, 0.0f),
            glm::vec4(-0.084193f, -0.106105f, -0.160369f, 0.0f),
            glm::vec4( 1.353743f,  1.566089f,  1.847385f, 0.0f),
            glm::vec4( 0.065199f,  0.074574f,  0.087086f, 0.0f),
            glm::vec4( 0.020387f,  0.023414f,  0.026277f, 0.0f),
            glm::vec4( 0.280597f,  0.325054f,  0.384689f, 0.0f),
            glm::vec4(-0.096101f, -0.117145f, -0.163413f, 0.0f),
            glm::vec4( 0.245422f,  0.283225f,  0.332972f, 0.0f) 
        },
        .brdf_lut_index = brdf_lut_index_,
        .max_reflection_lod = 5.0f,
        .shadow_map_index = shadow_map_index_,
        ._pad = {0.0f}
    };
    return ubo;
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
        
        if (instance.material.type == MaterialType::PBR) {
            // PBRペイロードとして書き込む
            // data[0..3] = base_color
            // data[4] = metallic
            // data[5] = roughness
            // data[6] = albedo_texture_id
            // data[7] = normal_texture_id
            // data[8] = mrm_texture_id
            std::memcpy(&object_data[instance_idx].data[0], &instance.material.pbr.base_color, sizeof(glm::vec4));
            std::memcpy(&object_data[instance_idx].data[4], &instance.material.pbr.metallic, sizeof(float));
            std::memcpy(&object_data[instance_idx].data[5], &instance.material.pbr.roughness, sizeof(float));
            object_data[instance_idx].data[6] = instance.material.pbr.albedo_texture_id;
            object_data[instance_idx].data[7] = instance.material.pbr.normal_texture_id;
            object_data[instance_idx].data[8] = instance.material.pbr.mrm_texture_id;
        } else if (instance.material.type == MaterialType::Toon) {
            // Toonペイロード
            // data[0..3] = base_color
            // data[4..7] = shade_color
            // data[8] = outline_width
            // data[9] = threshold
            // data[10] = feather
            // data[11] = albedo_texture_id
            // data[12] = shade_texture_id
            std::memcpy(&object_data[instance_idx].data[0], &instance.material.toon.base_color, sizeof(glm::vec4));
            std::memcpy(&object_data[instance_idx].data[4], &instance.material.toon.shade_color, sizeof(glm::vec4));
            std::memcpy(&object_data[instance_idx].data[8], &instance.material.toon.outline_width, sizeof(float));
            std::memcpy(&object_data[instance_idx].data[9], &instance.material.toon.threshold, sizeof(float));
            std::memcpy(&object_data[instance_idx].data[10], &instance.material.toon.feather, sizeof(float));
            object_data[instance_idx].data[11] = instance.material.toon.albedo_texture_id;
            object_data[instance_idx].data[12] = instance.material.toon.shade_texture_id;
        }

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

    graph_builder.import_image(
        shadow_map_handle_,
        fg::ImageDescription{
            .width = 2048,
            .height = 2048,
            .format = VK_FORMAT_D32_SFLOAT,
        },
        fg::UsageType::Undefined);

    graph_builder.add_pass("ShadowPass")
        .write_image(shadow_map_handle_, fg::UsageType::DepthAttachment)
        .execute([this, &snapshot](const fg::PassContext& ctx) {
            VkCommandBuffer cmd = ctx.command_buffer();
            
            VkRenderingAttachmentInfo depth_attachment{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = ctx.get_image_view(shadow_map_handle_),
                .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {{{1.0f, 0}},},
            };
            
            const VkRenderingInfo rendering_info{
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = {.offset = {0, 0}, .extent = {2048, 2048}},
                .layerCount = 1,
                .colorAttachmentCount = 0,
                .pColorAttachments = nullptr,
                .pDepthAttachment = &depth_attachment,
            };

            vkCmdBeginRendering(cmd, &rendering_info);

            const VkViewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = 2048.0f, .height = 2048.0f,
                .minDepth = 0.0f, .maxDepth = 1.0f,
            };
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            const VkRect2D scissor{
                .offset = {0, 0},
                .extent = {2048, 2048},
            };
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            std::array<VkDescriptorSet, 1> bound_sets = { global_bindless_set_ };
            vkCmdBindDescriptorSets(
                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline_layout_, 0,
                static_cast<uint32_t>(bound_sets.size()), bound_sets.data(),
                0, nullptr);

            VkBuffer vertex_buffers[] = {global_vertex_buffer_.buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, vertex_buffers, offsets);
            vkCmdBindIndexBuffer(cmd, global_index_buffer_.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadow_pipeline_.pipeline);

            if (!snapshot.instances.empty()) {
                vkCmdDrawIndexedIndirect(
                    cmd,
                    indirect_buffer_.buffer,
                    0,
                    static_cast<uint32_t>(snapshot.instances.size()),
                    sizeof(VkDrawIndexedIndirectCommand));
            }

            vkCmdEndRendering(cmd);
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
                .clearValue = {{{0.1f, 0.1f, 0.11f, 1.0f}}},
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
                pipeline_layout_,
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
                // TODO: 実際は PBR, Toon, ToonOutline ごとにインスタンスをソートするか
                // IndirectCommand のオフセットを計算して、複数回 vkCmdDrawIndexedIndirect を呼び出す必要がある。
                // 現在は全て PBR として一括描画する
                
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pbr_pipeline_.pipeline);
                vkCmdDrawIndexedIndirect(
                    cmd, 
                    indirect_buffer_.buffer, 
                    0, 
                    static_cast<uint32_t>(snapshot.instances.size()),
                    sizeof(VkDrawIndexedIndirectCommand)
                );
            }

            // Skybox描画
            if (skybox_pipeline_.pipeline != VK_NULL_HANDLE) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skybox_pipeline_.pipeline);
                vkCmdDraw(cmd, 3, 1, 0, 0); // 頂点バッファなしでフルスクリーン三角形を描画
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

