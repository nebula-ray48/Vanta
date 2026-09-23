#include "graph_compiler.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <vector>

namespace vanta::render::fg {

namespace {

void translate_usage_to_sync_state(
    UsageType usage,
    VkPipelineStageFlags2& stage,
    VkAccessFlags2& access,
    VkImageLayout& layout) noexcept {
    switch (usage) {
        case UsageType::Undefined:
            stage = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            access = VK_ACCESS_2_NONE;
            layout = VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case UsageType::ColorAttachment:
            stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case UsageType::DepthAttachment:
            stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                    VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
            break;
        case UsageType::DepthRead:
            stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT;
            layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
            break;
        case UsageType::ShaderRead:
            stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT;
            layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case UsageType::ShaderWrite:
            stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case UsageType::ComputeRead:
            stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_READ_BIT;
            layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case UsageType::ComputeWrite:
            stage = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            access = VK_ACCESS_2_SHADER_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_GENERAL;
            break;
        case UsageType::TransferSrc:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            access = VK_ACCESS_2_TRANSFER_READ_BIT;
            layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case UsageType::TransferDst:
            stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            access = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        case UsageType::Present:
            stage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            access = VK_ACCESS_2_NONE;
            layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
    }
}

template <typename Handle>
uint64_t handle_key(Handle handle, bool buffer) noexcept {
    const uint64_t kind = buffer ? (uint64_t{1} << 63u) : 0;
    return kind | (static_cast<uint64_t>(handle.index) << 32u) | handle.generation;
}

void add_dependency(
    size_t from,
    size_t to,
    std::vector<std::vector<size_t>>& edges,
    std::vector<uint32_t>& indegrees) {
    if (from == to || std::find(edges[from].begin(), edges[from].end(), to) != edges[from].end()) {
        return;
    }
    edges[from].push_back(to);
    ++indegrees[to];
}

} // namespace

[[nodiscard]] std::expected<ExecutionPlan, EngineError> compile_graph(
    const RenderGraphData& graph_data) noexcept {
    const size_t pass_count = graph_data.passes.size();
    std::vector<std::vector<size_t>> edges(pass_count);
    std::vector<uint32_t> indegrees(pass_count, 0);
    std::unordered_map<uint64_t, size_t> last_writer;
    std::unordered_map<uint64_t, std::vector<size_t>> readers;

    const auto valid_image = [&graph_data](ImageHandle handle) {
        return handle.index < graph_data.images.size() &&
               graph_data.images[handle.index].handle.generation == handle.generation;
    };
    const auto valid_buffer = [&graph_data](BufferHandle handle) {
        return handle.index < graph_data.buffers.size() &&
               graph_data.buffers[handle.index].handle.generation == handle.generation;
    };
    const auto register_access = [&last_writer, &readers, &edges, &indegrees](
        uint64_t key, bool write, size_t pass_index) {
        if (const auto writer = last_writer.find(key); writer != last_writer.end()) {
            add_dependency(writer->second, pass_index, edges, indegrees);
        }
        if (write) {
            if (const auto reader_list = readers.find(key); reader_list != readers.end()) {
                for (const size_t reader : reader_list->second) {
                    add_dependency(reader, pass_index, edges, indegrees);
                }
                reader_list->second.clear();
            }
            last_writer[key] = pass_index;
        } else {
            readers[key].push_back(pass_index);
        }
    };

    for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
        const PassData& pass = graph_data.passes[pass_index];
        for (uint32_t index = 0; index < pass.read_images_count; ++index) {
            const PassResource& resource = graph_data.all_read_images[pass.read_images_offset + index];
            if (!valid_image(resource.handle)) {
                return std::unexpected(LegacyError("Frame graph contains an invalid image handle"));
            }
            register_access(handle_key(resource.handle, false), false, pass_index);
        }
        for (uint32_t index = 0; index < pass.write_images_count; ++index) {
            const PassResource& resource = graph_data.all_write_images[pass.write_images_offset + index];
            if (!valid_image(resource.handle)) {
                return std::unexpected(LegacyError("Frame graph contains an invalid image handle"));
            }
            register_access(handle_key(resource.handle, false), true, pass_index);
        }
        for (uint32_t index = 0; index < pass.read_buffers_count; ++index) {
            const PassBufferResource& resource = graph_data.all_read_buffers[pass.read_buffers_offset + index];
            if (!valid_buffer(resource.handle)) {
                return std::unexpected(LegacyError("Frame graph contains an invalid buffer handle"));
            }
            register_access(handle_key(resource.handle, true), false, pass_index);
        }
        for (uint32_t index = 0; index < pass.write_buffers_count; ++index) {
            const PassBufferResource& resource = graph_data.all_write_buffers[pass.write_buffers_offset + index];
            if (!valid_buffer(resource.handle)) {
                return std::unexpected(LegacyError("Frame graph contains an invalid buffer handle"));
            }
            register_access(handle_key(resource.handle, true), true, pass_index);
        }
    }

    std::priority_queue<size_t, std::vector<size_t>, std::greater<>> ready;
    for (size_t pass_index = 0; pass_index < pass_count; ++pass_index) {
        if (indegrees[pass_index] == 0) {
            ready.push(pass_index);
        }
    }

    std::vector<size_t> order;
    order.reserve(pass_count);
    while (!ready.empty()) {
        const size_t pass_index = ready.top();
        ready.pop();
        order.push_back(pass_index);
        for (const size_t dependent : edges[pass_index]) {
            if (--indegrees[dependent] == 0) {
                ready.push(dependent);
            }
        }
    }
    if (order.size() != pass_count) {
        return std::unexpected(LegacyError("Frame graph contains a dependency cycle"));
    }

    ExecutionPlan plan{};
    plan.sorted_passes.reserve(pass_count);
    plan.sorted_pass_indices.reserve(pass_count);
    plan.barriers_per_pass.resize(pass_count);

    std::unordered_map<uint64_t, UsageType> resource_states;
    for (const ImageResource& image : graph_data.images) {
        if (image.image != VK_NULL_HANDLE) {
            resource_states[handle_key(image.handle, false)] = image.initial_usage;
        }
    }

    const auto append_barrier = [&resource_states, &plan](
        size_t sorted_index,
        std::variant<ImageHandle, BufferHandle> resource,
        UsageType new_usage) {
        const bool is_buffer = std::holds_alternative<BufferHandle>(resource);
        const uint64_t key = std::visit(
            [is_buffer](const auto handle) { return handle_key(handle, is_buffer); }, resource);
        const auto state = resource_states.find(key);
        const UsageType old_usage = state == resource_states.end() ? UsageType::Undefined : state->second;
        if (old_usage == new_usage) {
            return;
        }

        VkPipelineStageFlags2 old_stage{};
        VkPipelineStageFlags2 new_stage{};
        VkAccessFlags2 old_access{};
        VkAccessFlags2 new_access{};
        VkImageLayout old_layout{};
        VkImageLayout new_layout{};
        translate_usage_to_sync_state(old_usage, old_stage, old_access, old_layout);
        translate_usage_to_sync_state(new_usage, new_stage, new_access, new_layout);
        if (is_buffer) {
            old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            new_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }

        plan.barriers_per_pass[sorted_index].push_back(ResourceBarrier{
            .resource = resource,
            .before = old_usage,
            .after = new_usage,
            .src_stage = old_stage,
            .dst_stage = new_stage,
            .src_access = old_access,
            .dst_access = new_access,
            .old_layout = old_layout,
            .new_layout = new_layout,
        });
        resource_states[key] = new_usage;
    };

    for (size_t sorted_index = 0; sorted_index < order.size(); ++sorted_index) {
        const PassData& pass = graph_data.passes[order[sorted_index]];
        plan.sorted_passes.push_back(pass);
        plan.sorted_pass_indices.push_back(static_cast<uint32_t>(order[sorted_index]));
        for (uint32_t index = 0; index < pass.read_images_count; ++index) {
            const PassResource& resource = graph_data.all_read_images[pass.read_images_offset + index];
            append_barrier(sorted_index, resource.handle, resource.usage);
        }
        for (uint32_t index = 0; index < pass.write_images_count; ++index) {
            const PassResource& resource = graph_data.all_write_images[pass.write_images_offset + index];
            append_barrier(sorted_index, resource.handle, resource.usage);
        }
        for (uint32_t index = 0; index < pass.read_buffers_count; ++index) {
            const PassBufferResource& resource = graph_data.all_read_buffers[pass.read_buffers_offset + index];
            append_barrier(sorted_index, resource.handle, resource.usage);
        }
        for (uint32_t index = 0; index < pass.write_buffers_count; ++index) {
            const PassBufferResource& resource = graph_data.all_write_buffers[pass.write_buffers_offset + index];
            append_barrier(sorted_index, resource.handle, resource.usage);
        }
    }

    return plan;
}

} // namespace vanta::render::fg