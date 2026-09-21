#include "vulkan/resources/resource_registry.h"
#include "include/ext/vk_mem_alloc.h"

namespace vanta::render {

struct VulkanContext {
    VkDevice device;
    VmaAllocator allocator;
};

[[nodiscard]] std::expected<ImageHandle, std::string> ResourceRegistry::create_image(
    const VulkanContext& ctx, const ImageDescription& desc) {

    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = desc.type;
    image_info.format = desc.format;
    image_info.extent = {desc.width, desc.height, 1};
    image_info.mipLevels = desc.mip_levels;
    image_info.arrayLayers = desc.array_layers;
    image_info.samples = desc.samples;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = desc.usage;
    image_info.flags = desc.flags;

    // VMAの設定
    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_AUTO;

    // Transient（一時的）なリソースの場合、VMAの専用メモリを優先するなどの設定が可能ですが、
    // まずは標準的な設定にします。
    if (desc.ownership == ResourceOwnership::TRANSIENT) {
        alloc_info.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    VkImage new_image = VK_NULL_HANDLE;
    VmaAllocation new_allocation = VK_NULL_HANDLE;

    if (vmaCreateImage(ctx.allocator, &image_info, &alloc_info, &new_image, &new_allocation, nullptr) != VK_SUCCESS) {
        return std::unexpected("VMAによる画像の作成に失敗しました。");
    }

    // TODO: ここで VkImageView も作成して vk_image_views_ に保存する処理を追加します。
    VkImageView new_view = VK_NULL_HANDLE;

    ImageHandle handle;

    // 空き場所リスト（free_image_indices_）に要素があれば、そこを再利用する
    if (!free_image_indices_.empty()) {
        handle.index = free_image_indices_.back();
        free_image_indices_.pop_back();

        // 同じ場所を再利用するため、世代を1つ進める
        handle.generation = ++image_generations_[handle.index];

        image_descs_[handle.index] = desc;
        vk_images_[handle.index] = new_image;
        vk_image_views_[handle.index] = new_view;
        image_allocations_[handle.index] = new_allocation;
    } else {
        // 空き場所がない場合、配列の末尾に新しく追加する
        handle.index = static_cast<uint32_t>(vk_images_.size());
        handle.generation = 1;

        image_generations_.push_back(handle.generation);
        image_descs_.push_back(desc);
        vk_images_.push_back(new_image);
        vk_image_views_.push_back(new_view);
        image_allocations_.push_back(new_allocation);
    }

    return handle;
}

ImageHandle ResourceRegistry::register_imported_image(
    VkImage image, VkImageView view, const ImageDescription& desc) {

    ImageHandle handle;

    if (!free_image_indices_.empty()) {
        handle.index = free_image_indices_.back();
        free_image_indices_.pop_back();
        handle.generation = ++image_generations_[handle.index];

        image_descs_[handle.index] = desc;
        vk_images_[handle.index] = image;
        vk_image_views_[handle.index] = view;
        image_allocations_[handle.index] = VK_NULL_HANDLE;
    } else {
        handle.index = static_cast<uint32_t>(vk_images_.size());
        handle.generation = 1;

        image_generations_.push_back(handle.generation);
        image_descs_.push_back(desc);
        vk_images_.push_back(image);
        vk_image_views_.push_back(view);
        image_allocations_.push_back(VK_NULL_HANDLE);
    }

    return handle;
}

void ResourceRegistry::destroy_image(const VulkanContext& ctx, ImageHandle handle) {
    // ハンドルが有効か、世代が一致しているかを確認する
    if (!handle.is_valid() || handle.index >= vk_images_.size() || image_generations_[handle.index] != handle.generation) {
        return;
    }

    // VMAによる割り当てが存在する場合のみ、解放処理を行う（Importedな画像は解放しない）
    if (image_allocations_[handle.index] != VK_NULL_HANDLE) {
        if (vk_image_views_[handle.index] != VK_NULL_HANDLE) {
            vkDestroyImageView(ctx.device, vk_image_views_[handle.index], nullptr);
        }
        vmaDestroyImage(ctx.allocator, vk_images_[handle.index], image_allocations_[handle.index]);
    }

    // 配列の要素を空（無効）の状態にする
    vk_images_[handle.index] = VK_NULL_HANDLE;
    vk_image_views_[handle.index] = VK_NULL_HANDLE;
    image_allocations_[handle.index] = VK_NULL_HANDLE;

    // 次に新しい画像が作られる際、このインデックスを再利用できるようにリストへ追加する
    free_image_indices_.push_back(handle.index);
}

VkImage ResourceRegistry::get_vk_image(ImageHandle handle) const noexcept {
    if (!handle.is_valid() || handle.index >= vk_images_.size() || image_generations_[handle.index] != handle.generation) {
        return VK_NULL_HANDLE;
    }
    return vk_images_[handle.index];
}

}
