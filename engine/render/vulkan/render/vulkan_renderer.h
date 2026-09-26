//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

/**
 * @file vulkan_renderer.h
 * @brief VulkanRenderer クラスの宣言と、GPU 転送用の内部型定義
 *
 * このファイルは Vanta エンジンの「レンダラー全体」の外部から見えるインターフェースを定義します。
 *
 * =============================================
 * ファイル構成（.h と .cpp の分割）
 * =============================================
 *
 * vulkan_renderer.h          (このファイル)
 *   └─ クラス宣言、メンバ変数、内部型 (GpuObjectData, GpuMesh など)
 *
 * vulkan_renderer_core.cpp
 *   └─ 役割: Vulkan コンテキスト (VkDevice, スワップチェーンなど) の作成・破棄、
 *            GLTFシーンのロード (load_scene)、メッシュのアップロード (create_mesh_from_data)
 *   └─ 何かを触る場面:
 *       - 新しいリソースタイプ（バッファ・テクスチャなど）を初期化する
 *       - GLTFロードのロジックを変更・拡張する
 *       - デバイス/スワップチェーン設定を変更する
 *
 * vulkan_renderer_init.cpp
 *   └─ 役割: デスクリプタセット、パイプライン、シェーダーモジュールなどの初期化
 *            initialize_descriptor_resources()、initialize_pipeline_resources() が中心
 *   └─ 何かを触る場面:
 *       - 新しいシェーダー / パイプラインを追加する
 *       - デスクリプタセットのレイアウトやバインディングを変更する
 *       - テクスチャ/IBL/SSAO の初期化ロジックを変更する
 *
 * vulkan_renderer_draw.cpp
 *   └─ 役割: フレームごとの描画ループ。RenderSnapshot → GPU バッファへのパッキング、
 *            RenderGraph の構築と実行、ImGui フレームの管理
 *   └─ 何かを触る場面:
 *       - 新しいレンダリングパス（ポストプロセスなど）を追加する
 *       - マテリアルのGPUパッキングロジックを変更する（draw_frame の Phase 2 ループ）
 *       - 既存の描画順序や条件を変更する
 *
 * =============================================
 * パイプラインの役割一覧
 * =============================================
 *
 * pbr_pipeline_          : PBR マテリアルの本体描画（背面カリング、深度書き込みあり）
 * toon_pipeline_         : Toon マテリアルの本体描画（背面カリング、深度書き込みあり）
 * toon_outline_pipeline_ : Toon アウトライン描画（前面カリング＝裏面だけ描画、深度書き込みなし）
 * skybox_pipeline_       : 環境マップの背景描画（頂点バッファなし、全画面三角形）
 * shadow_pipeline_       : シャドウマップ生成（深度のみ、2048x2048）
 * depth_normal_pipeline_ : SSAO 用の深度・法線プリパス
 * tonemap_pipeline_      : HDR→SDR トーンマッピング（ポストプロセス）
 * bloom_extract_pipeline_: Bloom の明るいピクセル抽出
 * bloom_blur_pipeline_   : Bloom の横方向+縦方向ガウスぼかし
 * ssao_pipeline_         : SSAO 計算（コンピュートシェーダー）
 */
//------------------------------------------------//

#pragma once

#include <vulkan/vulkan.h>

#include <expected>
#include <vector>
#include <array>

#include "engine_error.h"
#include "render_types.h"
#include "vulkan/resources/buffers/vulkan_buffer_utils.h"
#include "vulkan/commands/command_recorder.h"
#include "vulkan/resources/descriptors/descriptor.h"
#include "vulkan/resources/images/texture.h"
#include "vulkan/pipeline/pipeline.h"
#include "vulkan//render/swapchain_target.h"
#include "vulkan/frame/frame_context.h"
#include "vulkan/resources/resource_registry.h"

namespace vanta::render {

    /**
     * @brief フレームごとの一時的な状態を保持する構造体
     *
     * begin_frame() の戻り値として返され、end_frame() まで保持します。
     * CommandRecorder 経由でコマンドバッファへの記録を行います。
     */
    struct ActiveFrame {
        CommandRecorder recorder;
        uint32_t image_index{0};
        uint32_t frame_index{0};
    };

    /// GPUに転送済みのメッシュデータ（グローバルバッファ内の位置情報）
    struct GpuMesh {
        uint32_t first_index;   ///< グローバルインデックスバッファ内の開始インデックス
        uint32_t index_count;   ///< このメッシュのインデックス数
        int32_t  vertex_offset; ///< グローバル頂点バッファ内のオフセット（vkCmdDrawIndexedIndirect の vertexOffset に対応）
    };

    /**
     * @brief GPUに転送する1オブジェクト分のデータ
     *
     * object_buffer_ (Storage Buffer) に詰め込まれ、シェーダー側の ObjectData 構造体と対応します。
     * data[32] は汎用ペイロードで、マテリアルの種類に応じて以下のように使われます:
     *
     * PBR マテリアルのレイアウト (vulkan_renderer_draw.cpp の Phase 2 参照):
     *   data[0..3]  = base_color (vec4)
     *   data[4]     = metallic
     *   data[5]     = roughness
     *   data[6]     = normal_scale
     *   data[7]     = occlusion_strength
     *   data[8]     = albedo_texture_id
     *   data[9]     = normal_texture_id
     *   data[10]    = mrm_texture_id
     *   data[11]    = emissive_texture_id
     *   data[12]    = occlusion_texture_id
     *
     * Toon マテリアルのレイアウト (vulkan_renderer_draw.cpp の Phase 2 参照):
     *   data[0..3]  = base_color (vec4)
     *   data[4..7]  = shade_color (vec4)
     *   data[8]     = outline_width
     *   data[9]     = threshold
     *   data[10]    = feather
     *   data[11]    = normal_scale
     *   data[12]    = albedo_texture_id
     *   data[13]    = shade_texture_id
     *   data[14]    = normal_texture_id
     *   data[15..18]= outline_color (vec4)
     *
     * シェーダー側での対応: common_types.slang の unpackPbrMaterial() / unpackToonMaterial()
     */
    struct alignas(16) GpuObjectData {
        glm::mat4 model_matrix;
        uint32_t data[32]; ///< 汎用ペイロード (128 bytes)
    };

    /**
     * @class VulkanRenderer
     * @brief Vantaエンジンのメインレンダラークラス
     *
     * Vulkan の初期化、リソース管理、描画パスの構築と実行を担当します。
     * 実装は3つの .cpp ファイルに分割されています（ファイル先頭の説明を参照）。
     *
     * 典型的な使い方:
     * @code
     *   auto renderer = VulkanRenderer::create(config).value();
     *   auto mesh_id  = renderer.create_mesh_from_data(mesh_data).value();
     *
     *   // ゲームループ内:
     *   RenderSnapshot snapshot = build_snapshot(mesh_id, ...);
     *   renderer.draw_frame(snapshot);
     * @endcode
     */
    class VulkanRenderer {
    public:
        /// 非同期のファクトリ関数。Vulkan の初期化が全て完了した後に返ります。
        [[nodiscard]] static std::expected<VulkanRenderer, EngineError> create(
            const RendererConfig& config);

        ~VulkanRenderer();

        VulkanRenderer(VulkanRenderer&& other) noexcept;
        VulkanRenderer& operator=(VulkanRenderer&& other) noexcept;
        VulkanRenderer(const VulkanRenderer&) = delete;
        VulkanRenderer& operator=(const VulkanRenderer&) = delete;

        // =============================================
        // 公開 API
        // =============================================

        /**
         * @brief ロード済みシーンノード（メッシュ1つ分）
         *
         * load_scene() の戻り値。mesh_id を RenderInstance.mesh_id にセットして
         * RenderSnapshot に追加することで描画できます。
         */
        struct LoadedSceneNode {
            MeshId mesh_id;
            MaterialData material;              ///< GLTFから読み込んだマテリアル情報（テクスチャIDなど）
            glm::mat4 global_transform{1.0f};   ///< ノードのワールド変換行列
        };

        /// 頂点・インデックスデータを GPU へアップロードし MeshId を返す
        [[nodiscard]] std::expected<MeshId, EngineError> create_mesh_from_data(const MeshData& data);

        /// フレームを描画する。毎フレーム呼び出す主要関数
        [[nodiscard]] std::expected<void, EngineError> draw_frame(const RenderSnapshot& snapshot);

        /// ウィンドウリサイズ時に呼ぶ。スワップチェーンとポストプロセスバッファを再生成する
        [[nodiscard]] std::expected<void, EngineError> resize(uint32_t width, uint32_t height);

        /// GLTF ファイルをロードし、全メッシュ・マテリアルを登録して LoadedSceneNode のリストを返す
        [[nodiscard]] std::expected<std::vector<LoadedSceneNode>, std::string> load_scene(const std::string& filepath);

        /// Texture オブジェクトをバインドレスシステムに登録し、シェーダーで使えるテクスチャIDを返す
        [[nodiscard]] std::expected<uint32_t, EngineError> register_texture(vanta::vulkan::Texture&& texture);

        // ImGui フレーム管理（テストアプリ用）
        void begin_imgui_frame();
        void end_imgui_frame(VkCommandBuffer cmd);

        /// ポストプロセス設定への参照（リアルタイムに変更可能）
        [[nodiscard]] PostProcessSettings& post_process_settings() { return post_process_settings_; }
        [[nodiscard]] const PostProcessSettings& post_process_settings() const { return post_process_settings_; }

    private:
        VulkanRenderer() = default;

        // フレーム開始・終了（スワップチェーンイメージ取得・提出）
        [[nodiscard]] std::expected<ActiveFrame, EngineError> begin_frame() const;
        [[nodiscard]] std::expected<void, EngineError> end_frame(const ActiveFrame& active_frame);

        // 初期化サブルーチン（vulkan_renderer_init.cpp で実装）
        [[nodiscard]] std::expected<void, EngineError> initialize_descriptor_resources();
        [[nodiscard]] std::expected<void, EngineError> initialize_pipeline_resources();

        /// RenderSnapshot から GlobalUbo を組み立てる（カメラ・ライト・SSAO 定数など）
        [[nodiscard]] GlobalUbo build_global_ubo(const RenderSnapshot& snapshot) const;

        // =============================================
        // プライベートメンバ変数
        // =============================================

        // --- 基盤システム ---
        RendererConfig config_;
        VulkanContext context_;                     ///< VkDevice, VkQueue, VmaAllocator など Vulkan 基盤
        SwapchainTarget swapchain_target_;          ///< スワップチェーン（表示用イメージ列）
        PostProcessSettings post_process_settings_{};

        // --- パイプライン群（全て同じ pipeline_layout_ を共有）---
        // 新しいパイプラインを追加する場合:
        //   1. ここにメンバを追加する
        //   2. vulkan_renderer_core.cpp の move コンストラクタ / 代入演算子に追加する
        //   3. vulkan_renderer_core.cpp の デストラクタに destroy を追加する
        //   4. vulkan_renderer_init.cpp の initialize_pipeline_resources() で作成する
        VkPipelineLayout pipeline_layout_{VK_NULL_HANDLE};
        GraphicsPipeline pbr_pipeline_;
        GraphicsPipeline toon_pipeline_;
        GraphicsPipeline toon_outline_pipeline_;    ///< Toon アウトライン（反転法線拡大法）
        GraphicsPipeline skybox_pipeline_;
        GraphicsPipeline shadow_pipeline_;
        GraphicsPipeline depth_normal_pipeline_;    ///< SSAO 用プリパス（深度+法線）
        GraphicsPipeline tonemap_pipeline_;
        GraphicsPipeline bloom_extract_pipeline_;
        GraphicsPipeline bloom_blur_pipeline_;
        ComputePipeline  ssao_pipeline_;

        // --- フレーム同期 ---
        std::array<FrameContext, MAX_FRAMES_IN_FLIGHT> frames_;
        uint32_t current_frame_index_{0};

        // --- メッシュ管理 ---
        std::vector<GpuMesh> meshes_;               ///< アップロード済みメッシュの位置情報

        // --- デスクリプタ関連 ---
        GpuBuffer global_ubo_buffer_;               ///< フレームごとに更新する GlobalUbo のバッファ

        // Bindless デスクリプタ (Set 0)
        // 全パイプラインが共有するセット。テクスチャ配列・サンプラー・オブジェクトバッファを含む
        VkDescriptorSetLayout bindless_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool bindless_pool_ = VK_NULL_HANDLE;
        VkDescriptorSet global_bindless_set_ = VK_NULL_HANDLE;

        // SSAO 専用デスクリプタ (Set 1)
        // SSAO コンピュートシェーダーが必要とする深度・法線テクスチャへのアクセスを提供
        VkDescriptorSetLayout ssao_layout_ = VK_NULL_HANDLE;
        VkDescriptorPool ssao_pool_ = VK_NULL_HANDLE;
        std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> ssao_sets_{VK_NULL_HANDLE};

        // ImGui 専用デスクリプタプール
        VkDescriptorPool imgui_pool_ = VK_NULL_HANDLE;
        [[nodiscard]] std::expected<void, EngineError> initialize_imgui();

        // --- テクスチャ管理 ---
        std::expected<void, EngineError> initialize_textures();
        std::vector<vanta::vulkan::Texture> textures_;      ///< register_texture() で登録されたテクスチャ列（インデックスがシェーダーのID）
        std::optional<vanta::vulkan::Texture> env_cubemap_; ///< IBL 用環境キューブマップ
        std::optional<vanta::vulkan::Texture> ssao_noise_tex_;
        std::array<glm::vec4, 64> ssao_samples_{};
        uint32_t brdf_lut_index_ = 0;               ///< BRDF Look-Up Table のテクスチャID
        uint32_t shadow_map_index_ = 0;             ///< シャドウマップのテクスチャID

        [[nodiscard]] FrameContext& current_frame() noexcept { return frames_[current_frame_index_]; }

        // --- RenderGraph リソースレジストリ ---
        ResourceRegistry registry_;                 ///< フレームグラフが管理する一時イメージ/バッファのレジストリ
        std::vector<ImageHandle> swapchain_image_handles_;
        ImageHandle shadow_map_handle_;

        // --- グローバル頂点・インデックスバッファ ---
        // 全メッシュの頂点とインデックスを1つの大きなバッファにまとめて格納する
        // vkCmdDrawIndexedIndirect で first_index + vertex_offset を指定して切り分ける
        GpuBuffer global_vertex_buffer_;
        GpuBuffer global_index_buffer_;
        uint32_t global_vertex_count_ = 0;
        uint32_t global_index_count_ = 0;
        uint32_t index_count_ = 0;

        // --- インダイレクト描画バッファ ---
        // CPU が毎フレーム書き込み、GPU が vkCmdDrawIndexedIndirect で読み取る
        GpuBuffer object_buffer_;   ///< GpuObjectData の配列（マテリアル・モデル行列）
        GpuBuffer indirect_buffer_; ///< VkDrawIndexedIndirectCommand の配列
        [[nodiscard]] std::expected<void, EngineError> initialize_draw_buffers();
    };

}  // namespace vanta::render

