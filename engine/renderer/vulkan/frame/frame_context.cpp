#include "frame_context.h"
#include "vulkan/core/vulkan_context.h"

namespace vanta::render {

std::expected<void, EngineError> FrameContext::initialize(const VulkanContext& ctx, uint32_t index) {
    frame_index = index;

    VkCommandPoolCreateInfo const pool_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = ctx.graphics_queue_family_index
    };

    if (vkCreateCommandPool(ctx.device, &pool_info, nullptr, &graphics_command_pool) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to create frame command pool"));
    }

    VkCommandBufferAllocateInfo alloc_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = graphics_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    if (vkAllocateCommandBuffers(ctx.device, &alloc_info, &graphics_command_buffer) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to allocate frame command buffer"));
    }

    if (vkCreateCommandPool(ctx.device, &pool_info, nullptr, &compute_command_pool) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to create compute command pool"));
    }

    alloc_info.commandPool = compute_command_pool;
    if (vkAllocateCommandBuffers(ctx.device, &alloc_info, &compute_command_buffer) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to allocate compute command buffer"));
    }

    VkSemaphoreCreateInfo const semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };
    VkFenceCreateInfo const fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT // 初回待機をパスするためシグナル状態で作成
    };

    if (vkCreateSemaphore(ctx.device, &semaphore_info, nullptr, &image_available) != VK_SUCCESS ||
        vkCreateSemaphore(ctx.device, &semaphore_info, nullptr, &render_finished) != VK_SUCCESS ||
        vkCreateFence(ctx.device, &fence_info, nullptr, &completion_fence) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to create frame sync objects"));
    }

    return {};
}

void FrameContext::destroy(const VulkanContext& ctx) noexcept {
    if (completion_fence != VK_NULL_HANDLE) vkDestroyFence(ctx.device, completion_fence, nullptr);
    if (render_finished != VK_NULL_HANDLE) vkDestroySemaphore(ctx.device, render_finished, nullptr);
    if (image_available != VK_NULL_HANDLE) vkDestroySemaphore(ctx.device, image_available, nullptr);
    if (compute_command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(ctx.device, compute_command_pool, nullptr);
    if (graphics_command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(ctx.device, graphics_command_pool, nullptr);
    graphics_command_pool = VK_NULL_HANDLE;
    compute_command_pool = VK_NULL_HANDLE;
    graphics_command_buffer = VK_NULL_HANDLE;
    compute_command_buffer = VK_NULL_HANDLE;
}

void FrameContext::wait_for_previous_frame(const VulkanContext& ctx) const noexcept {
    vkWaitForFences(ctx.device, 1, &completion_fence, VK_TRUE, UINT64_MAX);
}

std::expected<void, EngineError> FrameContext::begin_command_recording(const VulkanContext& ctx) noexcept {
    vkResetFences(ctx.device, 1, &completion_fence);
    vkResetCommandPool(ctx.device, graphics_command_pool, 0);

    VkCommandBufferBeginInfo const begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    if (vkBeginCommandBuffer(graphics_command_buffer, &begin_info) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to begin recording command buffer"));
    }
    return {};
}

std::expected<void, EngineError> FrameContext::submit(const VulkanContext& ctx) noexcept {
    if (vkEndCommandBuffer(graphics_command_buffer) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to end command buffer"));
    }

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo const submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &image_available,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &graphics_command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &render_finished
    };

    if (vkQueueSubmit(ctx.graphics_queue, 1, &submit_info, completion_fence) != VK_SUCCESS) {
        return std::unexpected(LegacyError("Failed to submit frame command buffer"));
    }
    return {};
}

} // namespace vanta::render
