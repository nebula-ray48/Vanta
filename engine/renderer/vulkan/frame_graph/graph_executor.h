#pragma once
#include "vulkan/frame_graph/graph_compiler.h"
#include "vulkan/resources/resource_registry.h"
#include <vulkan/vulkan.h>
#include "vulkan/core/vulkan_context.h"
#include <expected>
#include <string>

namespace vanta::render::fg {

class GraphExecutor {
public:
    // コンパイラが生成した計画と、リソースの実体を管理するレジストリを受け取る
    [[nodiscard]] std::expected<void, std::string> execute(
        VkCommandBuffer cmd,
        const ExecutionPlan& plan,
        const RenderGraphData& graph_data,
        ResourceRegistry& registry,
        const VulkanContext& ctx
    ) const noexcept;

private:
    // 抽象バリア (ResourceBarrier) を Vulkan の VkImageMemoryBarrier2 に変換して発行する
    void issue_barriers(
        VkCommandBuffer cmd,
        const std::vector<ResourceBarrier>& barriers,
        const ResourceRegistry& registry,
        const std::unordered_map<uint64_t, ImageHandle>& handle_map
    ) const noexcept;
};

} // namespace vanta::render::fg
