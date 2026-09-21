#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <expected>
#include "include/engine_error.h"

namespace vanta::render {

constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

struct VulkanContext;

// 将来実装するフレーム単位の一時メモリアロケータ等のダミー
struct DescriptorArena {};
struct UploadArena {};
struct DeferredDestroyQueue {};

struct FrameContext {
    uint32_t frame_index{0};

    // フレーム専用のコマンドプール（リセット時に他のフレームに干渉しない）
    VkCommandPool graphics_command_pool{VK_NULL_HANDLE};
    VkCommandBuffer graphics_command_buffer{VK_NULL_HANDLE};

    VkCommandPool compute_command_pool{VK_NULL_HANDLE};
    VkCommandBuffer compute_command_buffer{VK_NULL_HANDLE};

    // 同期オブジェクト
    VkFence completion_fence{VK_NULL_HANDLE};
    VkSemaphore image_available{VK_NULL_HANDLE};
    VkSemaphore render_finished{VK_NULL_HANDLE};

    DescriptorArena descriptor_arena;
    UploadArena upload_arena;
    DeferredDestroyQueue deferred_destroy;

    // --- メソッド ---
    [[nodiscard]] std::expected<void, EngineError> initialize(const VulkanContext& ctx, uint32_t index);
    void destroy(const VulkanContext& ctx) noexcept;

    // Swapchainの取得前に待機するためのメソッド
    void wait_for_previous_frame(const VulkanContext& ctx) const noexcept;

    // 取得成功後にフェンスとコマンドバッファをリセットして記録を開始する
    [[nodiscard]] std::expected<void, EngineError> begin_command_recording(const VulkanContext& ctx) noexcept;

    // キューへ送信する
    [[nodiscard]] std::expected<void, EngineError> submit(const VulkanContext& ctx) noexcept;
};

} // namespace vanta::render
