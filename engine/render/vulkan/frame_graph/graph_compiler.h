#pragma once

#include <expected>
#include "render_graph_types.h"
#include "engine_error.h"

namespace vanta::render::fg {

[[nodiscard]] std::expected<ExecutionPlan, EngineError> compile_graph(
    const RenderGraphData& graph_data
) noexcept;

} // namespace vanta::render::fg
