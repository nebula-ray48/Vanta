//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>

#include "../core/vulkan_context.h"

namespace vanta::render {

class CommandRecorder {
    public:
        VkDevice device_{VK_NULL_HANDLE};
        VkCommandBuffer command_buffer{VK_NULL_HANDLE};

        CommandRecorder() = default;
        CommandRecorder(VkDevice device, VkCommandBuffer cmd) noexcept
            : device_(device), command_buffer(cmd) {}

public:
    [[gnu::always_inline]] inline
    void bind_vertex_buffer(const  GpuBuffer& buffer) const noexcept {
        constexpr VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer.buffer, &offset);
    }

    [[gnu::always_inline]] inline
    void set_viewport(uint32_t first_viewport, std::span<const VkViewport> viewports) const noexcept {
        vkCmdSetViewport(command_buffer, first_viewport, static_cast<uint32_t>(viewports.size()), viewports.data());
    }

    [[gnu::always_inline]] inline
    void set_scissor(uint32_t first_scissor, std::span<const VkRect2D> scissors) const noexcept {
        vkCmdSetScissor(command_buffer, first_scissor, static_cast<uint32_t>(scissors.size()), scissors.data());
    }

    [[gnu::always_inline]] inline
    std::expected<void, std::string> bind_index_buffer(const GpuBuffer& buffer) const noexcept {
        if (!buffer.index_type.has_value()) {
            return std::unexpected("bind_index_buffer: 渡されたGpuBufferにindex_typeが設定されていません");
        }

        vkCmdBindIndexBuffer(command_buffer, buffer.buffer, 0, buffer.index_type.value());
        return {};
    }

    [[gnu::always_inline]] inline
    void draw_indexed(
        uint32_t index_count,
        uint32_t instance_count,
        uint32_t first_index,
        int32_t vertex_offset,
        uint32_t first_instance
    ) const noexcept {
        vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    [[gnu::always_inline]] inline
    void begin_render_pass(const VkRenderPassBeginInfo& begin_info) const noexcept {
        vkCmdBeginRenderPass(command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    }

    [[gnu::always_inline]] inline
    void end_render_pass() const noexcept {
        vkCmdEndRenderPass(command_buffer);
    }

    [[gnu::always_inline]] inline
    void bind_pipeline(VkPipeline pipeline) const noexcept {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }

    [[gnu::always_inline]] inline
    void push_constants(
        VkPipelineLayout layout,
        VkShaderStageFlags stage_flags,
        uint32_t offset,
        std::span<const std::byte> constants
    ) const noexcept {
        vkCmdPushConstants(
            command_buffer,
            layout,
            stage_flags,
            offset,
            static_cast<uint32_t>(constants.size_bytes()),
            constants.data()
        );
    }
};

}  // namespace vanta::render
