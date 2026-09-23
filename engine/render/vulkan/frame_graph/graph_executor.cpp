#include "graph_executor.h"
#include "vulkan/frame_graph/pass_context.h"
#include "vulkan/utils/vulkan_format_utils.h"

#include <variant>
#include <vector>

namespace vanta::render::fg {

namespace {

} // namespace

void GraphExecutor::issue_barriers(
	VkCommandBuffer cmd,
	const std::vector<ResourceBarrier>& barriers,
	const ResourceRegistry& registry,
    const std::unordered_map<uint64_t, ImageHandle>& handle_map) const noexcept {
	std::vector<VkImageMemoryBarrier2> image_barriers;
    std::vector<VkBufferMemoryBarrier2> buffer_barriers;
	image_barriers.reserve(barriers.size());

	for (const ResourceBarrier& barrier : barriers) {
	    if (std::holds_alternative<ImageHandle>(barrier.resource)) {
	        ImageHandle image_handle = std::get<ImageHandle>(barrier.resource);
            uint64_t key = (static_cast<uint64_t>(image_handle.index) << 32) | image_handle.generation;
            if (auto it = handle_map.find(key); it != handle_map.end()) {
                image_handle = it->second;
            }

	        const VkImage image = registry.get_vk_image(image_handle);
	        if (image == VK_NULL_HANDLE) {
	            continue;
	        }

	        const auto& desc = registry.get_image_desc(image_handle);
	        const VkImageAspectFlags aspect_mask = ::vanta::render::get_image_aspect_mask(desc.format);

	        image_barriers.push_back(VkImageMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                .srcStageMask = barrier.src_stage,
                .srcAccessMask = barrier.src_access,
                .dstStageMask = barrier.dst_stage,
                .dstAccessMask = barrier.dst_access,
                .oldLayout = barrier.old_layout,
                .newLayout = barrier.new_layout,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = image,
                .subresourceRange = {
                    aspect_mask,
                    0, desc.mip_levels > 0 ? desc.mip_levels : 1,
                    0, desc.array_layers > 0 ? desc.array_layers : 1,
                },
            });
	    } else if (std::holds_alternative<BufferHandle>(barrier.resource)) {
	        const BufferHandle buffer_handle = std::get<BufferHandle>(barrier.resource);
	        const VkBuffer buffer = registry.get_vk_buffer(buffer_handle);
	        if (buffer == VK_NULL_HANDLE) {
	            continue;
	        }

	        buffer_barriers.push_back(VkBufferMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                .srcStageMask = barrier.src_stage,
                .srcAccessMask = barrier.src_access,
                .dstStageMask = barrier.dst_stage,
                .dstAccessMask = barrier.dst_access,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .buffer = buffer,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
            });
	    }
	}

    // ループを抜けてから、集めたバリアを 1 回だけ発行する
	if (!image_barriers.empty() || !buffer_barriers.empty()) {
	    const VkDependencyInfo dependency_info{
	        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = static_cast<uint32_t>(buffer_barriers.size()),
            .pBufferMemoryBarriers = buffer_barriers.empty() ? nullptr : buffer_barriers.data(),
            .imageMemoryBarrierCount = static_cast<uint32_t>(image_barriers.size()),
            .pImageMemoryBarriers = image_barriers.empty() ? nullptr : image_barriers.data(),
        };
	    vkCmdPipelineBarrier2(cmd, &dependency_info);
	}
}

std::expected<void, std::string> GraphExecutor::execute(
	VkCommandBuffer cmd,
	const ExecutionPlan& plan,
	const RenderGraphData& graph_data,
	ResourceRegistry& registry,
    const VulkanContext& ctx) const noexcept {
	if (cmd == VK_NULL_HANDLE) {
		return std::unexpected("Frame graph command buffer is null");
	}
	if (plan.sorted_passes.size() != plan.barriers_per_pass.size()) {
		return std::unexpected("Frame graph execution plan has mismatched pass and barrier counts");
	}
	if (plan.sorted_passes.size() != plan.sorted_pass_indices.size()) {
		return std::unexpected("Frame graph execution plan has mismatched pass indices");
	}

    std::unordered_map<uint64_t, ImageHandle> handle_map;
    std::vector<ImageHandle> transient_images;

    for (const ImageResource& res : graph_data.images) {
        if ((res.handle.index & 0x80000000) != 0) {
            ::vanta::render::ImageDescription transient_desc{};
            transient_desc.width = res.description.width;
            transient_desc.height = res.description.height;
            transient_desc.format = res.description.format;
            transient_desc.usage = res.description.usage;
            transient_desc.type = VK_IMAGE_TYPE_2D;
            transient_desc.mip_levels = 1;
            transient_desc.array_layers = 1;
            transient_desc.samples = VK_SAMPLE_COUNT_1_BIT;
            transient_desc.ownership = ResourceOwnership::TRANSIENT;
            transient_desc.flags = 0;

            auto handle_res = registry.create_image(ctx, transient_desc);
            if (handle_res) {
                uint64_t key = (static_cast<uint64_t>(res.handle.index) << 32) | res.handle.generation;
                handle_map[key] = *handle_res;
                transient_images.push_back(*handle_res);
            }
        }
    }

	for (size_t pass_index = 0; pass_index < plan.sorted_passes.size(); ++pass_index) {
		issue_barriers(cmd, plan.barriers_per_pass[pass_index], registry, handle_map);
		const PassData& pass = plan.sorted_passes[pass_index];
		if (pass.execute) {
			PassContext pass_ctx{cmd, registry, handle_map};
			pass.execute(pass_ctx);
		}
	}

    for (ImageHandle h : transient_images) {
        registry.destroy_image(ctx, h);
    }

	return {};
}

} // namespace vanta::render::fg
