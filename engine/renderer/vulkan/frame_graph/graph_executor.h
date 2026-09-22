#pragma once
#include "vulkan/frame_graph/graph_compiler.h"
#include "vulkan/resources/resource_registry.h"
#include <vulkan/vulkan.h>
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
        const ResourceRegistry& registry
    ) const noexcept;

private:
    // 抽象バリア (ResourceBarrier) を Vulkan の VkImageMemoryBarrier2 に変換して発行する
    void issue_barriers(
        VkCommandBuffer cmd,
        const std::vector<ResourceBarrier>& barriers,
        const ResourceRegistry& registry
    ) const noexcept;
};

} // namespace vanta::render::fg
