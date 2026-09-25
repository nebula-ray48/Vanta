# Vanta Engine Architecture Overview

このドキュメントでは、Vanta Engine の描画コア（Vulkan Renderer）の設計アーキテクチャについて解説します。
本エンジンは、現代的な **AAA級レンダリングアーキテクチャ** を目指し、以下の3つの主要なパラダイムを採用しています。

1. **GPU-Driven Rendering (Bindless Architecture)**
2. **Render Graph (Frame Graph)**
3. **Dynamic Rendering (Vulkan 1.3)**

---

## 1. GPU-Driven Rendering (Bindless Architecture)

従来のグラフィックスAPIでは、オブジェクトを描画するたびに「Descriptor Set の切り替え（バインド）」や「Push Constants の更新」を CPU から発行する必要があり、これが大きなオーバーヘッドになっていました。
Vanta Engine では **Bindless Architecture** を採用し、これらのCPUオーバーヘッドを最小化しています。

### Descriptor Set 0 の単一化
`vulkan/resources/descriptors/descriptor.cpp` にて、全てのシェーダーが共有する巨大な Descriptor Set 0 を定義しています。
* **Binding 0**: `GlobalUbo` (カメラのビュー行列、投影行列など)
* **Binding 1**: `Sampled Image` (エンジン内のすべてのテクスチャを配列として保持、`UpdateAfterBind` 対応)
* **Binding 2**: `Sampler` (共通のサンプラー)
* **Binding 3**: `Storage Buffer` (全オブジェクトのモデル行列等を持つ `GpuObjectData` 配列)

### Indirect Draw の活用
CPUは描画ループ (`vulkan_renderer_draw.cpp`) において、オブジェクトごとのデータを `GpuObjectData` (SSBO) に書き込み、描画コマンドを `VkDrawIndexedIndirectCommand` バッファに書き込みます。
実際の描画は `vkCmdDrawIndexedIndirect` を用いて GPU にコマンドバッファを解釈させることで行います。頂点シェーダーでは `SV_InstanceID` をキーにして SSBO から自分自身のデータを取得します。

---

## 2. Render Graph (Frame Graph)

描画パス（シャドウパス、メインカラーパス、ポストプロセスなど）間の複雑な依存関係や、メモリバリア（Synchronization）を自動化するための仕組みです。

### 動作フロー
1. **パスの宣言 (Builder)**: `RenderGraphBuilder` を使って、「どのパスが、どの仮想リソース（Image/Buffer）を、どういう用途（Read/Write）で使うか」を宣言します。
2. **依存関係の解決 (Compiler)**: `compile_graph` がパスの実行順序を並び替え、リソースの状態遷移（Layout Transition）やメモリバリアを自動計算して `RenderPlan` を生成します。
3. **パスの実行 (Executor)**: `GraphExecutor` が `RenderPlan` に従い、実際の Vulkan API（バリア発行や `vkCmdBeginRendering` など）を呼び出し、ユーザーが定義したコールバック処理を実行します。

開発者は「どのリソースを使うか」を宣言するだけでよく、バグの温床となる `VkImageMemoryBarrier` の手動管理から解放されます。

---

## 3. Dynamic Rendering

Vulkan 1.3 のコア機能である `VK_KHR_dynamic_rendering` を利用しています。
従来の Vulkan にあった巨大で複雑な `VkRenderPass` や `VkFramebuffer` の事前生成を廃止し、描画時に直接 `VkRenderingInfo` を使ってアタッチメントを指定するモダンな手法です。
これにより、Render Graph との相性が非常に良くなり、動的なパス構成の変更が容易になっています。

---

## 主要ディレクトリとファイルの役割

* `engine/render/vulkan/render/`
    * `vulkan_renderer.h` / `core.cpp`: エンジン初期化、リソース管理、終了処理。
    * `vulkan_renderer_init.cpp`: Vulkan のボイラープレート初期化処理。
    * `vulkan_renderer_draw.cpp`: 毎フレームの描画ループ。Render Graph の構築とコンパイルを行う。
* `engine/render/vulkan/frame_graph/`
    * Render Graph アーキテクチャのコア。`builder`, `compiler`, `executor` に分かれている。
* `engine/render/vulkan/resources/`
    * Buffer, Image, Descriptor のラッパー。`descriptor.cpp` に Bindless の設定が集約されている。
* `engine/render/vulkan/pipeline/`
    * `PipelineBuilder` を用いたグラフィックスパイプラインの構築。
