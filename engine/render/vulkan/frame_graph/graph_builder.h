#pragma once

#include <string_view>
#include <utility>
#include "render_graph_types.h"

namespace vanta::render::fg {

    /**
     * @class PassBuilder
     * @brief 個別の描画・計算パスが使用するリソース（Image/Buffer）とその読み書き状態を定義するビルダー。
     * 
     * この情報を元に、Render Graphコンパイラは最適なバリア（同期処理）を自動計算します。
     * 最後に `execute()` メソッドで、実際の描画処理（コールバック関数）を登録します。
     */
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

    /**
     * @class RenderGraphBuilder
     * @brief Render Graphの全体を構築するビルダー。
     * 
     * リソース（Virtual Image / Virtual Buffer）の生成やインポート、および
     * 各パス（PassBuilder）の追加を行います。
     * 最終的に `build()` することで、実行可能な `RenderGraphData` を生成します。
     */
    class RenderGraphBuilder {
public:
    RenderGraphBuilder() noexcept = default;

    [[nodiscard]] ImageHandle create_image(const ImageDescription& description) noexcept;
    [[nodiscard]] ImageHandle import_image(
        VkImage image,
        const ImageDescription& description,
        UsageType initial_usage = UsageType::PRESENT) noexcept;

    void import_image(
        ImageHandle handle,
        const ImageDescription& description,
        UsageType initial_usage) noexcept;
    [[nodiscard]] BufferHandle create_buffer(const BufferDescription& description) noexcept;

    PassBuilder add_pass(std::string_view name) noexcept;

    [[nodiscard]] RenderGraphData build() noexcept {
        return std::move(graph_data_);
    }

private:
    RenderGraphData graph_data_;
};

}  // namespace vanta::render::fg
