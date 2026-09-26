//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

/**
 * @file render_types.h
 * @brief エンジン全体で共有する「CPU側」のデータ型を定義するファイル
 *
 * ここにあるデータ型は、ゲームロジック層（tests/ や将来の editor/）と
 * レンダラー層（engine/render/）の間のインターフェースを担います。
 *
 * --- データの流れ ---
 *   ゲームロジック
 *     ↓ RenderInstance / RenderSnapshot を組み立てる (tests/test_main.cpp)
 *   VulkanRenderer::draw_frame()
 *     ↓ GpuObjectData に変換してGPUバッファへ書き込む (vulkan_renderer_draw.cpp)
 *   シェーダー
 *     ↓ ObjectData として受け取り、unpackXxxMaterial() で展開する (common_types.slang)
 *
 * --- 新しいマテリアルを追加するときの手順 ---
 *   1. このファイルに XxxMaterialParams 構造体を追加する
 *   2. MaterialType enum に新しい値を追加する
 *   3. MaterialData に新しい Params メンバを追加する
 *   4. common_types.slang に対応するシェーダー側の型と unpack 関数を追加する
 *   5. vulkan_renderer_draw.cpp の draw_frame() 内のパッキングループに処理を追加する
 *   6. 新しいシェーダーファイル (.slang) を assets/shaders/materials/ に作成する
 *   7. CMakeLists.txt の generate_shaders ターゲットにコンパイル命令を追加する
 *   8. vulkan_renderer_init.cpp の initialize_pipeline_resources() で新パイプラインを作成する
 *   9. vulkan_renderer.h にパイプラインメンバ変数を追加する
 *  10. vulkan_renderer_draw.cpp の MainColorPass 内で新パイプラインを使って描画する
 */

#pragma once

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <cstddef>

// ============================================================
// マテリアルパイプライン戦略
// RendererConfig に設定することで、レンダラーが作成するパイプラインを制御します。
// PBR_Only     : PBRパイプラインのみ作成（Toon関連の初期化コストを省く）
// PBR_Toon_Hybrid: PBR + Toon + Toon_Outline の3つのパイプラインを作成する
// ============================================================
enum class MaterialPipelineStrategy : uint32_t {
    PBR_Only = 0,
    PBR_Toon_Hybrid = 1
};

/**
 * @brief レンダラーの初期化設定
 *
 * VulkanRenderer::create() に渡します。
 * - 新しいパイプライン戦略を追加した場合は material_strategy に値を追加する
 * - ウィンドウサイズの変更は resize() 経由で行う（ここは初期値のみ）
 */
struct RendererConfig {
    MaterialPipelineStrategy material_strategy = MaterialPipelineStrategy::PBR_Toon_Hybrid;
    const char* app_name = "Vanta Engine";
    void* window_handle = nullptr;
    uint32_t window_width = 800;
    uint32_t window_height = 600;
};

// ============================================================
// ID 型
// エンティティとメッシュを区別するための型安全なラッパー
// MeshId は VulkanRenderer::create_mesh_from_data() で返ってくる値を保存する
// ============================================================
struct EntityId { uint32_t value; };
struct MeshId   { uint32_t value; };

/**
 * @brief GPU に送る頂点データの1単位
 *
 * GLTFローダーや手動メッシュ生成で使われます。
 * シェーダー側の VertexInput 構造体 (common_types.slang) と
 * レイアウトが一致するよう get_binding_description() と get_attribute_descriptions() で定義されています。
 *
 * 新しい頂点属性を追加するには:
 *   1. ここにメンバを追加する
 *   2. get_attribute_descriptions() に VkVertexInputAttributeDescription を追加する
 *   3. common_types.slang の VertexInput に対応するフィールドを追加する
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
    glm::vec3 normal;
    glm::vec2 uv;
    uint32_t texture_id = 0;

    static vk::VertexInputBindingDescription get_binding_description() {
        return vk::VertexInputBindingDescription()
            .setBinding(0)
            .setStride(sizeof(Vertex))
            .setInputRate(vk::VertexInputRate::eVertex);
    }

    static std::array<vk::VertexInputAttributeDescription, 5> get_attribute_descriptions() {
        return {
            vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, position)),
            vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat,     offsetof(Vertex, uv)),
            vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32Uint,          offsetof(Vertex, texture_id))
        };
    }
};

/**
 * @brief CPU 上のメッシュデータ（頂点 + インデックス配列）
 *
 * VulkanRenderer::create_mesh_from_data() に渡すと GPU バッファにアップロードされ、
 * 対応する MeshId が返ってきます。
 * 手続き的メッシュ生成の static メソッドを追加するのに適した場所です。
 */
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // TODO: キューブや平面の生成ロジックは、MeshDataのstaticメソッドとして実装
    static MeshData new_cube(float size, glm::vec3 color);
    static MeshData new_plane(float width, float depth, glm::vec3 color);
};

// ============================================================
// マテリアル型
// 新しいマテリアルを追加する際は、ここに enum 値と Params 構造体を追加する
// ============================================================
enum class MaterialType : uint32_t {
    PBR  = 0, ///< 物理ベースレンダリング。metallic/roughness ワークフロー
    Toon = 1  ///< セルルックシェーディング。アウトライン描画を含む
};

/**
 * @brief PBR マテリアルのパラメータ (CPU 側)
 *
 * シェーダー側の対応型: PbrMaterialData (common_types.slang)
 * GPU へのパッキング: vulkan_renderer_draw.cpp の "1. PBR Instances" ループ
 *
 * テクスチャIDは VulkanRenderer::register_texture() で取得した値を使う。
 * 0 はデフォルトテクスチャ（白1x1）を示す特別な値。
 */
struct PbrMaterialParams {
    glm::vec4 base_color{1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float normal_scale = 1.0f;
    float occlusion_strength = 1.0f;
    uint32_t albedo_texture_id = 0;     ///< sRGB カラーテクスチャ
    uint32_t normal_texture_id = 0;     ///< 法線マップ（UNORM）
    uint32_t mrm_texture_id = 0;        ///< Metallic/Roughness/Ambient Occlusion (UNORM)
    uint32_t emissive_texture_id = 0;   ///< 自己発光テクスチャ（sRGB）
    uint32_t occlusion_texture_id = 0;  ///< オクルージョンテクスチャ（UNORM）
};

/**
 * @brief Toon マテリアルのパラメータ (CPU 側)
 *
 * シェーダー側の対応型: ToonMaterialData (common_types.slang)
 * GPU へのパッキング: vulkan_renderer_draw.cpp の "2. Toon Instances" ループ
 *
 * アウトライン描画は反転法線拡大法（Inverted Hull）で実装されており、
 * toon_outline_pipeline_ による別パスで描画されます。
 * outline_width は世界空間での拡大量に変換係数（0.002）をかけた値で制御されます。
 */
struct ToonMaterialParams {
    glm::vec4 base_color{1.0f};                          ///< ベース色（テクスチャにかけ算される乗数）
    glm::vec4 shade_color{0.5f, 0.5f, 0.5f, 1.0f};      ///< 影色のベース
    glm::vec4 outline_color{0.05f, 0.05f, 0.05f, 1.0f}; ///< アウトラインの色
    float outline_width = 1.0f;   ///< アウトラインの太さ（0=なし、大きいほど太い）
    float threshold = 0.5f;       ///< 明暗境界の位置（0=全体が影、1=全体が明）
    float feather = 0.1f;         ///< 明暗境界のぼかし幅（0=ハード、大きいほどソフト）
    float normal_scale = 1.0f;    ///< 法線マップの強さ
    uint32_t albedo_texture_id = 0;
    uint32_t shade_texture_id = 0;
    uint32_t normal_texture_id = 0;
};

/**
 * @brief あるオブジェクトの「マテリアル全体」を表すコンテナ
 *
 * type フィールドで使うマテリアル種別を選択し、
 * 対応する pbr または toon のパラメータを設定します。
 * draw_frame() がこの type を見て正しいパイプラインに割り振ります。
 *
 * 将来的には std::variant<PbrMaterialParams, ToonMaterialParams> にリファクタリング予定
 */
struct MaterialData {
    MaterialType type = MaterialType::PBR;

    // TODO: C++17 std::variant を使用してPBRとToonのパラメータを保持する
    PbrMaterialParams pbr;
    ToonMaterialParams toon;
};

/**
 * @brief 1フレームに描画する1オブジェクトの情報
 *
 * ゲームロジック側は毎フレーム RenderInstance のリストを組み立て、
 * RenderSnapshot に詰めて draw_frame() に渡します。
 */
struct RenderInstance {
    EntityId entity_id;       ///< (現在は未使用。将来的にエンティティ追跡に使う)
    MeshId mesh_id;           ///< create_mesh_from_data() で取得したID
    glm::mat4 model_matrix;   ///< ワールド変換行列（位置・回転・スケール）
    MaterialData material;    ///< このオブジェクトのマテリアル設定
};

/**
 * @brief 1フレーム分のレンダリングに必要な全データをまとめたスナップショット
 *
 * ゲームロジックとレンダラーの主要なインターフェース。
 * ゲームループの1フレームごとに生成してから draw_frame() に渡します。
 * マルチスレッド化を想定して「スナップショット」という名前になっています。
 *
 * カメラ行列の分割 (view / proj / viewProj) は SSAO などの
 * スクリーンスペースエフェクトで個別に必要になるため別々に保持します。
 */
struct RenderSnapshot {
    uint64_t frame_number;
    std::vector<RenderInstance> instances;
    glm::mat4 view_proj_matrix; ///< view * proj（基本描画に使う）
    glm::mat4 view_matrix;      ///< SSAO、Depth Normal Pass で個別に必要
    glm::mat4 proj_matrix;      ///< SSAO で個別に必要
    glm::vec3 camera_pos{0.0f, 0.0f, 0.0f};
    glm::vec3 sun_direction{0.2f, 0.5f, 1.0f};
};

// ============================================================
// 以下は GPU 転送用の補助構造体。通常ゲームロジック側からは直接使わない
// ============================================================

struct alignas(16) GpuTransform {
    glm::vec3 position;
    float _pad0;
    glm::vec4 rotation;
    glm::vec3 scale;
    float _pad1;
};

struct alignas(16) GpuEntity {
    uint32_t id;
    uint32_t mesh_id;
    uint32_t _pad0[2];
    GpuTransform transform;
};

struct PushConstants {
    glm::mat4 mvp;
    PushConstants(const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {
        mvp = proj * view * model;
    }
};

/**
 * @brief ポストプロセス設定
 *
 * VulkanRenderer::post_process_settings() で参照を取得して書き換えることで
 * リアルタイムに設定変更が反映されます。
 *
 * 新しいポストプロセスエフェクトを追加する際の手順:
 *   1. ここにパラメータを追加する
 *   2. vulkan_renderer_draw.cpp の draw_frame() に新しいパスを追加する
 *   3. 対応するシェーダーを assets/shaders/postprocess/ に作成する
 *   4. CMakeLists.txt にコンパイル命令を追加する
 *   5. vulkan_renderer_init.cpp にパイプライン生成コードを追加する
 *   6. vulkan_renderer.h にパイプラインメンバ変数を追加する
 */
struct PostProcessSettings {
    // --- 全般 ---
    float render_scale = 1.0f;   ///< 内部解像度スケール (0.5 = 半分の解像度で描画し拡大)
    bool enable_ssao = true;     ///< Screen Space Ambient Occlusion（隙間の影）
    bool enable_bloom = true;    ///< Bloom（強い光のにじみ）
    bool enable_shadows = true;  ///< シャドウマップによる影
    bool enable_skybox = true;   ///< 環境キューブマップの背景描画

    // --- 環境・露出 ---
    float exposure = 1.0f;          ///< EV（露出値）。トーンマップ時に適用される
    float skybox_intensity = 1.0f;  ///< スカイボックスの明るさ倍率
    float ibl_intensity = 1.0f;     ///< IBL（Image-Based Lighting）の強さ

    // --- Bloom ---
    float bloom_threshold = 3.0f;       ///< この輝度以上のピクセルだけがブルームを発する
    float bloom_soft_knee = 0.5f;       ///< 閾値の境界のソフト度（0=ハード、1=ソフト）
    float streak_length = 4.0f;         ///< アナモルフィックフレアの長さ（横方向の光のすじ）
    float bloom_tint[3] = {0.35f, 0.75f, 1.35f}; ///< ブルームの色合い (RGB)
    float bloom_intensity = 0.35f;      ///< ブルームの全体的な強さ

    // --- トーンマップ・映像的レンズ効果 ---
    float ca_strength = 0.0012f;      ///< 色収差（Chromatic Aberration）の強さ
    float saturation = 1.05f;         ///< 彩度（1.0=変化なし）
    float contrast = 1.04f;           ///< コントラスト（1.0=変化なし）
    float vignette_radius = 1.15f;    ///< ビネット（周辺減光）の半径
    float vignette_smoothness = 0.65f;///< ビネットのグラデーションの滑らかさ
    float grain_amount = 0.008f;      ///< フィルムグレインの強さ
};

