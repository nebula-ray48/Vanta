#include "graph_builder.h"

namespace vanta::render::fg {

ImageHandle RenderGraphBuilder::create_image(const ImageDescription& description) noexcept {
    // 仮想ハンドルは上位ビットを立てる (0x80000000)
    const uint32_t virtual_index = static_cast<uint32_t>(graph_data_.images.size()) | 0x80000000;
    const ImageHandle handle{
        .index = virtual_index,
        .generation = 1,
    };
    graph_data_.images.push_back(ImageResource{
        .handle = handle,
        .description = description,
        .image = VK_NULL_HANDLE,
        .initial_usage = UsageType::Undefined,
    });
    return handle;
}

ImageHandle RenderGraphBuilder::import_image(
    VkImage image,
    const ImageDescription& description,
    UsageType initial_usage) noexcept {
    const ImageHandle handle{
        .index = static_cast<uint32_t>(graph_data_.images.size()),
        .generation = 1,
    };
    graph_data_.images.push_back(ImageResource{
        .handle = handle,
        .description = description,
        .image = image,
        .initial_usage = initial_usage,
    });
    return handle;
}

void RenderGraphBuilder::import_image(
    ImageHandle handle,
    const ImageDescription& description,
    UsageType initial_usage) noexcept {
    // フラットな配列として追加するだけ
    graph_data_.images.push_back(ImageResource{
        .handle = handle,
        .description = description,
        .image = VK_NULL_HANDLE, // Persistent resources are managed externally
        .initial_usage = initial_usage,
    });
}

BufferHandle RenderGraphBuilder::create_buffer(const BufferDescription& description) noexcept {
    const BufferHandle handle{
        .index = static_cast<uint32_t>(graph_data_.buffers.size()),
        .generation = 1,
    };
    graph_data_.buffers.push_back(BufferResource{
        .handle = handle,
        .description = description,
    });
    return handle;
}

PassBuilder& PassBuilder::read_image(ImageHandle handle, UsageType usage) noexcept {
    graph_.all_read_images.push_back(PassResource{handle, usage});
    pass_.read_images_count++;

    return *this;
}

PassBuilder& PassBuilder::write_image(ImageHandle handle, UsageType usage) noexcept {
    graph_.all_write_images.push_back(PassResource{handle, usage});
    pass_.write_images_count++;
    return *this;
}

PassBuilder& PassBuilder::read_buffer(BufferHandle handle, UsageType usage) noexcept {
    graph_.all_read_buffers.push_back(PassBufferResource{handle, usage});
    pass_.read_buffers_count++;
    return *this;
}

PassBuilder& PassBuilder::write_buffer(BufferHandle handle, UsageType usage) noexcept {
    graph_.all_write_buffers.push_back(PassBufferResource{handle, usage});
    pass_.write_buffers_count++;
    return *this;
}

void PassBuilder::execute(PassData::ExecuteFunc func) noexcept {
    pass_.execute = func;
}

PassBuilder RenderGraphBuilder::add_pass(std::string_view name) noexcept {
    PassData new_pass{};

    new_pass.read_images_offset = static_cast<uint32_t>(graph_data_.all_read_images.size());
    new_pass.write_images_offset = static_cast<uint32_t>(graph_data_.all_write_images.size());
    new_pass.read_buffers_offset = static_cast<uint32_t>(graph_data_.all_read_buffers.size());
    new_pass.write_buffers_offset = static_cast<uint32_t>(graph_data_.all_write_buffers.size());

    graph_data_.passes.push_back(new_pass);
    graph_data_.pass_names.push_back(name);

    return PassBuilder(graph_data_, graph_data_.passes.back());
}

}  // namespace vanta::render::fg
