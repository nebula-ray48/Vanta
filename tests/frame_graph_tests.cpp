#include <gtest/gtest.h>

#include <cstdint>

#include "vulkan/frame_graph/graph_builder.h"
#include "vulkan/frame_graph/graph_compiler.h"

namespace vanta::render::fg {

TEST(FrameGraph, SortsWriteBeforeReadDependency) {
    RenderGraphBuilder builder;
    const BufferHandle buffer = builder.create_buffer(BufferDescription{.size = 64});

    builder.add_pass("write").write_buffer(buffer, UsageType::TRANSFER_DST);
    builder.add_pass("read").read_buffer(buffer, UsageType::COMPUTE_READ);

    const auto plan = compile_graph(builder.build());

    ASSERT_TRUE(plan.has_value());
    ASSERT_EQ(plan->sorted_pass_indices.size(), 2U);
    EXPECT_EQ(plan->sorted_pass_indices[0], 0U);
    EXPECT_EQ(plan->sorted_pass_indices[1], 1U);
}

TEST(FrameGraph, GeneratesColorToPresentBarrierForImportedImage) {
    RenderGraphBuilder builder;
    const VkImage imported_image = reinterpret_cast<VkImage>(static_cast<uintptr_t>(1));
    const ImageHandle swapchain = builder.import_image(
        imported_image,
        ImageDescription{.width = 1280, .height = 720, .format = VK_FORMAT_B8G8R8A8_UNORM});

    builder.add_pass("color").write_image(swapchain, UsageType::WRITE_COLOR);
    builder.add_pass("present").read_image(swapchain, UsageType::PRESENT);

    const auto plan = compile_graph(builder.build());

    ASSERT_TRUE(plan.has_value());
    ASSERT_EQ(plan->barriers_per_pass.size(), 2U);
    ASSERT_EQ(plan->barriers_per_pass[1].size(), 1U);

    const ResourceBarrier& barrier = plan->barriers_per_pass[1][0];
    ASSERT_TRUE(std::holds_alternative<ImageHandle>(barrier.resource));
    EXPECT_EQ(std::get<ImageHandle>(barrier.resource).id, swapchain.id);
    EXPECT_EQ(std::get<ImageHandle>(barrier.resource).generation, swapchain.generation);
    EXPECT_EQ(barrier.before, UsageType::ColorAttachment);
    EXPECT_EQ(barrier.after, UsageType::Present);
    EXPECT_EQ(barrier.old_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    EXPECT_EQ(barrier.new_layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    EXPECT_EQ(barrier.src_access, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    EXPECT_EQ(barrier.dst_access, VK_ACCESS_2_NONE);
}

TEST(FrameGraph, GeneratesAbstractBufferBarrier) {
    RenderGraphBuilder builder;
    const BufferHandle buffer = builder.create_buffer(BufferDescription{.size = 256});

    builder.add_pass("upload").write_buffer(buffer, UsageType::TransferDst);
    builder.add_pass("read").read_buffer(buffer, UsageType::ShaderRead);

    const auto plan = compile_graph(builder.build());

    ASSERT_TRUE(plan.has_value());
    ASSERT_EQ(plan->barriers_per_pass.size(), 2U);
    ASSERT_EQ(plan->barriers_per_pass[1].size(), 1U);
    const ResourceBarrier& barrier = plan->barriers_per_pass[1][0];
    ASSERT_TRUE(std::holds_alternative<BufferHandle>(barrier.resource));
    EXPECT_EQ(std::get<BufferHandle>(barrier.resource).id, buffer.id);
    EXPECT_EQ(std::get<BufferHandle>(barrier.resource).generation, buffer.generation);
    EXPECT_EQ(barrier.before, UsageType::TransferDst);
    EXPECT_EQ(barrier.after, UsageType::ShaderRead);
    EXPECT_EQ(barrier.old_layout, VK_IMAGE_LAYOUT_UNDEFINED);
    EXPECT_EQ(barrier.new_layout, VK_IMAGE_LAYOUT_UNDEFINED);
}

TEST(FrameGraph, RejectsStaleImageGeneration) {
    RenderGraphBuilder builder;
    const ImageHandle image = builder.create_image(
        ImageDescription{.width = 64, .height = 64, .format = VK_FORMAT_R8G8B8A8_UNORM});
    const ImageHandle stale{.id = image.id, .generation = image.generation + 1};
    builder.add_pass("invalid").write_image(stale, UsageType::WRITE_COLOR);

    EXPECT_FALSE(compile_graph(builder.build()).has_value());
}

} // namespace vanta::render::fg