#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>
#include <expected>
#include <functional>
#include <string_view>
#include <variant>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include "engine_error.h"
#include "vulkan/resources/resource_registry.h"

namespace vanta::render::fg {

struct ImageHandle {
    uint32_t id{0};
    uint32_t generation{0};
};

struct BufferHandle {
    uint32_t id{0};
    uint32_t generation{0};
};

struct ImageDescription {
    uint32_t width = 0;
    uint32_t height = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    // 影の場合は VK_FORMAT_D32_SFLOAT などを指定します
};

struct BufferDescription {
    VkDeviceSize size{0};
};

enum class UsageType : std::uint8_t {
    Undefined,
    ColorAttachment,
    DepthAttachment,
    DepthRead,
    ShaderRead,
    ShaderWrite,
    TransferSrc,
    TransferDst,
    Present,
    ComputeRead,
    ComputeWrite,

    // Legacy names retained while call sites migrate to the canonical names.
    READ_TEXTURE = ShaderRead,
    READ_DEPTH = DepthRead,
    WRITE_DEPTH = DepthAttachment,
    WRITE_COLOR = ColorAttachment,
    PRESENT = Present,
    COMPUTE_READ = ComputeRead,
    COMPUTE_WRITE = ComputeWrite,
    TRANSFER_SRC = TransferSrc,
    TRANSFER_DST = TransferDst,
};

struct PassResource {
    ImageHandle handle;
    UsageType usage;
};

struct PassBufferResource {
    BufferHandle handle;
    UsageType usage;
};

struct ShadowData {
    glm::mat4 light_view_matrix;
    glm::mat4 light_projection_matrix;
};

class PassContext;

struct PassData {
    uint32_t read_images_offset = 0;
    uint32_t read_images_count = 0;
    uint32_t write_images_offset = 0;
    uint32_t write_images_count = 0;

    uint32_t read_buffers_offset = 0;
    uint32_t read_buffers_count = 0;
    uint32_t write_buffers_offset = 0;
    uint32_t write_buffers_count = 0;

    using ExecuteFunc = std::function<void(const PassContext&)>;
    ExecuteFunc execute = nullptr;
};

struct ResourceBarrier {
    std::variant<ImageHandle, BufferHandle> resource;

    UsageType before;
    UsageType after;

    VkPipelineStageFlags2 src_stage;
    VkPipelineStageFlags2 dst_stage;
    VkAccessFlags2 src_access;
    VkAccessFlags2 dst_access;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
};

struct ExecutionPlan {
    std::vector<PassData> sorted_passes;
    std::vector<uint32_t> sorted_pass_indices;
    std::vector<std::vector<ResourceBarrier>> barriers_per_pass;
};

struct ImageResource {
    ImageHandle handle;
    ImageDescription description;
    VkImage image{VK_NULL_HANDLE};
    UsageType initial_usage{UsageType::Undefined};
};

struct BufferResource {
    BufferHandle handle;
    BufferDescription description;
};

struct RenderGraphData {
    std::vector<PassData> passes;
    std::vector<std::string_view> pass_names;

    std::vector<ImageResource> images;
    std::vector<BufferResource> buffers;

    // ImageHandle ではなく PassResource を敷き詰める
    std::vector<PassResource> all_read_images;
    std::vector<PassResource> all_write_images;

    // バッファの依存関係も敷き詰める
    std::vector<PassBufferResource> all_read_buffers;
    std::vector<PassBufferResource> all_write_buffers;
};


// ==========================================
// シーン全体データ (Global)
// カメラや太陽の光、ゲーム内時間など、全員が共通で使うデータ
// ==========================================
struct GlobalSceneData {
    glm::mat4 view_projection;
    glm::vec3 camera_position;
    float time;                   // アニメーションや揺れる草などの計算用

    glm::vec3 main_light_dir;     // 太陽の向き
    float main_light_intensity;   // 光の強さ
};

// ==========================================
// マテリアルデータ (Material)
// ==========================================
struct MaterialData {
    glm::vec4 base_color_factor{1.0f};
    float metallic_factor = 0.0f;
    float roughness_factor = 1.0f;

    // トゥーン用の特殊パラメータ
    float toon_shadow_step = 0.5f;   // 影のパキッと感の境界線
    float toon_outline_width = 1.0f; // 輪郭線の太さ
};

// ==========================================
// インスタンスデータ (静的オブジェクト)
// ==========================================
struct InstanceData {
    glm::mat4 model_matrix;
    uint32_t mesh_id;      // どの形を描くか
    uint32_t material_id;  // どの色を塗るか
    uint32_t padding[2];   // 16バイトの区切りを綺麗にするための余白（Vulkanのお約束）
};

// ==========================================
// アニメーションデータ (スキンメッシュ)
// ==========================================
struct SkinnedInstanceData {
    uint32_t instance_id; // 基本的な位置やマテリアルは InstanceData を参照する
    uint32_t bone_offset; // 巨大なボーン行列配列の「どこから」自分の骨格データを読むか
    uint32_t bone_count;  // 骨の数
    uint32_t padding;     // 余白
};

// ==========================================
// ポストプロセスデータ (色調補正・エフェクト)
// ==========================================
struct PostProcessData {
    float exposure = 1.0f;        // 露出（カメラの明るさ）
    float gamma = 2.2f;           // ガンマ補正
    float bloom_threshold = 1.0f; // どこから光を溢れさせるか（ブルーム）
    float bloom_intensity = 0.5f; // 溢れる光の強さ
};

}  // namespace vanta::render::fg
