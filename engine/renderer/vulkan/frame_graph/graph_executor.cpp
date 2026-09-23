#include "graph_executor.h"

#include <variant>
#include <vector>

namespace vanta::render::fg {

namespace {

::vanta::render::ImageHandle to_registry_handle(ImageHandle handle) noexcept {
	return ::vanta::render::ImageHandle{
		.index = handle.id,
		.generation = handle.generation,
	};
}

::vanta::render::BufferHandle to_registry_handle(BufferHandle handle) noexcept {
	return ::vanta::render::BufferHandle{
		.index = handle.id,
		.generation = handle.generation,
	};
}

} // namespace

void GraphExecutor::issue_barriers(
	VkCommandBuffer cmd,
	const std::vector<ResourceBarrier>& barriers,
	const ResourceRegistry& registry) const noexcept {
	std::vector<VkImageMemoryBarrier2> image_barriers;
    std::vector<VkBufferMemoryBarrier2> buffer_barriers;
	image_barriers.reserve(barriers.size());

	for (const ResourceBarrier& barrier : barriers) {
	    if (std::holds_alternative<ImageHandle>(barrier.resource)) {
	        const ImageHandle image_handle = std::get<ImageHandle>(barrier.resource);
	        const VkImage image = registry.get_vk_image(to_registry_handle(image_handle));
	        if (image == VK_NULL_HANDLE) {
	            continue;
	        }

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
                    barrier.after == UsageType::DepthAttachment
                        ? VK_IMAGE_ASPECT_DEPTH_BIT
                        : VK_IMAGE_ASPECT_COLOR_BIT,
                    0, 1, 0, 1,
                },
            });
	    } else if (std::holds_alternative<BufferHandle>(barrier.resource)) {
	        const BufferHandle buffer_handle = std::get<BufferHandle>(barrier.resource);
	        const VkBuffer buffer = registry.get_vk_buffer(to_registry_handle(buffer_handle));
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
	const ResourceRegistry& registry) const noexcept {
	if (cmd == VK_NULL_HANDLE) {
		return std::unexpected("Frame graph command buffer is null");
	}
	if (plan.sorted_passes.size() != plan.barriers_per_pass.size()) {
		return std::unexpected("Frame graph execution plan has mismatched pass and barrier counts");
	}
	if (plan.sorted_passes.size() != plan.sorted_pass_indices.size()) {
		return std::unexpected("Frame graph execution plan has mismatched pass indices");
	}

	for (size_t pass_index = 0; pass_index < plan.sorted_passes.size(); ++pass_index) {
		issue_barriers(cmd, plan.barriers_per_pass[pass_index], registry);
		const PassData& pass = plan.sorted_passes[pass_index];
		if (pass.execute) {
			pass.execute(cmd);
		}
	}

	(void)graph_data;
	return {};
}

} // namespace vanta::render::fg
