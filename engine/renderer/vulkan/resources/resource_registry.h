#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <expected>

#include "include/ext/vk_mem_alloc.h"

namespace vanta::render {

// Forward declarations (VulkanDeviceやVMAアロケータを持つコンテキスト構造体)
struct VulkanContext;

// Handles
struct ImageHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != UINT32_MAX;
    }
    constexpr bool operator==(const ImageHandle&) const = default;
};
constexpr ImageHandle NULL_IMAGE_HANDLE{UINT32_MAX, 0};


struct BufferHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != UINT32_MAX;
    }
    constexpr bool operator==(const BufferHandle&) const = default;
};
constexpr BufferHandle NULL_BUFFER_HANDLE{UINT32_MAX, 0};


// Descriptions & Enums

// リソースのライフサイクルと所有権
enum class ResourceOwnership : uint8_t {
    TRANSIENT,  // Frame Graphが生成・破棄を管理（フレーム内で再利用される可能性あり）
    PERSISTENT, // フレームを跨いで保持される（履歴バッファ等）
    IMPORTED,   // Swapchainなど外部が所有し、Graphが借りるだけ
    EXTERNAL    // アセットシステムなどが所有
};

struct ImageDescription {
    uint32_t width = 1;
    uint32_t height = 1;
    uint32_t mip_levels = 1;
    uint32_t array_layers = 1;

    VkFormat format = VK_FORMAT_UNDEFINED;
    VkImageType type = VK_IMAGE_TYPE_2D;
    VkImageUsageFlags usage = 0;
    VkImageCreateFlags flags = 0;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

    ResourceOwnership ownership = ResourceOwnership::TRANSIENT;
    std::string debug_name; // Vulkan Debug Marker 用
};

struct BufferDescription {
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;

    ResourceOwnership ownership = ResourceOwnership::TRANSIENT;
    std::string debug_name;
};


// Resource Registry

class ResourceRegistry {
public:
    ResourceRegistry() = default;
    ~ResourceRegistry() = default;

    // コピー禁止、ムーブ許可
    ResourceRegistry(const ResourceRegistry&) = delete;
    ResourceRegistry& operator=(const ResourceRegistry&) = delete;
    ResourceRegistry(ResourceRegistry&&) noexcept = default;
    ResourceRegistry& operator=(ResourceRegistry&&) noexcept = default;

    // --- Image API ---
    [[nodiscard]] std::expected<ImageHandle, std::string> create_image(
        const VulkanContext& ctx, const ImageDescription& desc);

    // スワップチェーン画像など、既存の VkImage を登録する
    ImageHandle register_imported_image(
        VkImage image, VkImageView view, const ImageDescription& desc);

    // リソースの破棄。Transientなものはフレーム終了時にGraphから呼ばれる
    void destroy_image(const VulkanContext& ctx, ImageHandle handle);

    // --- Image Accessors (Graph実行時の高速アクセス用) ---
    [[nodiscard]] VkImage get_vk_image(ImageHandle handle) const noexcept;
    [[nodiscard]] VkImageView get_vk_image_view(ImageHandle handle) const noexcept;
    [[nodiscard]] const ImageDescription& get_image_desc(ImageHandle handle) const noexcept;

    // --- Buffer API ---
    [[nodiscard]] std::expected<BufferHandle, std::string> create_buffer(
        const VulkanContext& ctx, const BufferDescription& desc);

    BufferHandle register_imported_buffer(VkBuffer buffer, const BufferDescription& desc);

    void destroy_buffer(const VulkanContext& ctx, BufferHandle handle);

    [[nodiscard]] VkBuffer get_vk_buffer(BufferHandle handle) const noexcept;
    [[nodiscard]] const BufferDescription& get_buffer_desc(BufferHandle handle) const noexcept;

    // エンジン終了時などの全破棄
    void clear_all(const VulkanContext& ctx);

private:
    // Data-Oriented Design: Structure of Arrays (SoA)
    // キャッシュ効率向上のため、フラットなベクターで管理する

    std::vector<uint32_t> image_generations_;
    std::vector<ImageDescription> image_descs_;
    std::vector<VkImage> vk_images_;
    std::vector<VkImageView> vk_image_views_;

    std::vector<VmaAllocation> image_allocations_;

    // ID再利用のためのフリーリスト
    std::vector<uint32_t> free_image_indices_;

    // Buffer SoA
    std::vector<uint32_t> buffer_generations_;
    std::vector<BufferDescription> buffer_descs_;
    std::vector<VkBuffer> vk_buffers_;
    std::vector<VmaAllocation> buffer_allocations_;
    std::vector<uint32_t> free_buffer_indices_;
};

} // namespace vanta::render
