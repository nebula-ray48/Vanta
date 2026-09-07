#pragma once

#include <string_view>
#include <utility>
#include "render_graph_types.h"

namespace vanta::render::fg {

class PassBuilder {
public:
    PassBuilder(RenderGraphData& graph, PassData& pass) noexcept
        : graph_(graph), pass_(pass) {}

    PassBuilder& read_image(ImageHandle handle, UsageType usage) noexcept;
    PassBuilder& write_image(ImageHandle handle, UsageType usage) noexcept;

    PassBuilder& read_buffer(BufferHandle handle, UsageType usage) noexcept;
    PassBuilder& write_buffer(BufferHandle handle, UsageType usage) noexcept;

    void execute(PassData::ExecuteFunc func) noexcept;

private:
    RenderGraphData& graph_;
    PassData& pass_;
};

class RenderGraphBuilder {
public:
    RenderGraphBuilder() noexcept = default;

    PassBuilder add_pass(std::string_view name) noexcept;

    [[nodiscard]] RenderGraphData build() noexcept {
        return std::move(graph_data_);
    }

private:
    RenderGraphData graph_data_;
};

}  // namespace vanta::render::fg
