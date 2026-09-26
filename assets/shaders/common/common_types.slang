// types.slang
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

/**
 * common_types.slang
 *
 * 全シェーダーから import される共通型定義モジュール。
 *
 * アーキテクチャのポイント:
 *   1. 全シェーダーは「 import common_types; 」でこの型を使う
 *   2. CPU側の対応型は engine/core/include/render_types.h を参照
 *
 * データの流れ:
 *   CPU (render_types.h) → GpuObjectData にパッキング
 *      (vulkan_renderer_draw.cpp の draw_frame 内ループ)
 *     ↓
 *   GPU (common_types.slang) → ObjectData の data[] をシェーダーが受け取る
 *     ↓
 *   unpackPbrMaterial() / unpackToonMaterial() で型安全な構造体に展開
 *
 * 新しいマテリアル型を追加する場合:
 *   1. XxxMaterialData 構造体をこのファイルに追加する
 *   2. unpackXxxMaterial() 関数を追加する
 *   3. CPU側の render_types.h に対応する XxxMaterialParams を追加する
 *   4. vulkan_renderer_draw.cpp のパッキングループに処理を追加する
 */

module common_types;

// 全シェーダー共通の頂点属性入力
// render_types.h の Vertex 構造体とレイアウトが忥必一致していること
// location ごとに vulkan_renderer_init.cpp の get_attribute_descriptions() と対応
// location(0) = position, (1) = color, (2) = normal, (3) = uv, (4) = textureId
public struct VertexInput {
    [[vk::location(0)]] public float3 position : POSITION;
    [[vk::location(1)]] public float3 color : COLOR;
    [[vk::location(2)]] public float3 normalDir : NORMAL;
    [[vk::location(3)]] public float2 uv : TEXCOORD;
    [[vk::location(4)]] public uint textureId : TEXINDEX;
};

// 頂点シェーダーからフラグメントシェーダーへの輸出値
// instanceId は nointerpolation（補間されない）で第1頂点の値をそのまま伝える
public struct VertexOutput {
    public float4 position : SV_Position;
    public float3 worldPos : WORLDPOS;
    public float3 color : COLOR;
    public float3 normalDir : NORMAL;
    public float2 uv : TEXCOORD;
    nointerpolation public uint instanceId : INSTANCEID;
};

/**
 * GlobalUbo: 全シェーダー共通の定数バッファ (Set 0, Binding 0)
 *
 * CPU側で build_global_ubo() (→ vulkan_renderer_draw.cpp) で毎フレーム更新される。
 * 新しい全体共通パラメータを追加する場合:
 *   1. ここにフィールドを追加する（パディングに注意）
 *   2. engine/render/include/shader_reflection.h の GlobalUbo 構造体に同じフィールドを追加する
 *   3. vulkan_renderer_draw.cpp の build_global_ubo() で値を準備する
 */
public struct GlobalUbo {
    public float4x4 viewProj;      ///< view * proj (基本描画に使う)
    public float4x4 invViewProj;   ///< SSAO のアンプロジェクションで使う
    public float4x4 lightViewProj; ///< シャドウマップ用のライト空間変換
    public float3 cameraPos;
    public float _padding;
    public float4 sunDirection;    ///< xyz=方向, w=強さ
    public float4 sunColor;
    public float4 ambientColor;
    public float4 sh[9];           ///< 球面調和関数係数（IBL の拡散間接光計算用）
    public uint brdfLutIndex;      ///< BRDF LUT のテクスチャID
    public float maxReflectionLod; ///< IBL のミップマップ層数
    public uint shadowMapIndex;    ///< シャドウマップテクスチャID (0=雑影無効)
    public uint ssaoMapIndex;      ///< SSAOテクスチャID (0=SSAO無効)
    public float4x4 viewMatrix;    ///< SSAO の view空間変換用
    public float4x4 projMatrix;    ///< SSAO の投影行列
    public float4x4 invProjMatrix; ///< SSAO のアンプロジェクション
    public float4   samples[64];   ///< SSAO サンプルカーネル
    public float2   screenSize;    ///< ピクセル単位の画面サイズ
    public float    radius;        ///< SSAO サンプリング半径
    public float    bias;          ///< SSAO まったいなアーティファクト防止用
    public float    iblIntensity;  ///< IBL全体の強さ
    public float    skyboxIntensity;
    public float2   _padding2;
};

/**
 * ObjectData: 1オブジェクト分の汎用GPUデータ (Set 0, Binding 3)
 *
 * CPU側の対応: GpuObjectData (vulkan_renderer.h)
 * data[] のレイアウトは GpuObjectData のコメントを参照
 */
public struct ObjectData {
    public float4x4 model_matrix;
    public uint data[32]; ///< マテリアルパラメータやテクスチャIDを格納する汎用バッファ
};

/**
 * PbrMaterialData: PBR マテリアルのシェーダー内表現
 *
 * CPU側の対応: PbrMaterialParams (render_types.h)
 * unpackPbrMaterial() で ObjectData から展開して使う。
 * PBR 計算: assets/shaders/materials/pbr_only/pbr_main.slang
 */
public struct PbrMaterialData {
    public float4 base_color;
    public float metallic;
    public float roughness;
    public float normal_scale;
    public float occlusion_strength;
    public uint albedo_texture_id;
    public uint normal_texture_id;
    public uint mrm_texture_id;        ///< Metallic/Roughness/AO が1テクスチャにまとまっている（M=Rチャンネル, R=G, AO=B）
    public uint emissive_texture_id;
    public uint occlusion_texture_id;
};

// ヘルパー関数: ObjectData から PbrMaterialData に変換する
public PbrMaterialData unpackPbrMaterial(ObjectData objData) {
    PbrMaterialData matData;
    matData.base_color = float4(asfloat(objData.data[0]), asfloat(objData.data[1]), asfloat(objData.data[2]), asfloat(objData.data[3]));
    matData.metallic = asfloat(objData.data[4]);
    matData.roughness = asfloat(objData.data[5]);
    matData.normal_scale = asfloat(objData.data[6]);
    matData.occlusion_strength = asfloat(objData.data[7]);
    matData.albedo_texture_id = objData.data[8];
    matData.normal_texture_id = objData.data[9];
    matData.mrm_texture_id = objData.data[10];
    matData.emissive_texture_id = objData.data[11];
    matData.occlusion_texture_id = objData.data[12];
    return matData;
}

/**
 * ToonMaterialData: Toon マテリアルのシェーダー内表現
 *
 * CPU側の対応: ToonMaterialParams (render_types.h)
 * unpackToonMaterial() で ObjectData から展開して使う。
 * チェーン: assets/shaders/materials/toon/toon_main.slang
 * アウトライン: assets/shaders/materials/toon/toon_outline.slang
 */
public struct ToonMaterialData {
    public float4 base_color;      ///< ベース色（テクスチャにコンポジットされる）
    public float4 shade_color;     ///< 影色
    public float4 outline_color;   ///< アウトラインの色
    public float  outline_width;   ///< アウトラインの太さ（ワールド空間拡大量に 0.002 をかけた値）
    public float  threshold;       ///< 明暗境界の位置（0=全面影、 1=全面明）
    public float  feather;         ///< 明暗境界のぼかし幅（0=ハード）
    public float  normal_scale;    ///< 法線マップの強さ
    public uint   albedo_texture_id;
    public uint   shade_texture_id;
    public uint   normal_texture_id;
};

// ヘルパー関数: ObjectData から ToonMaterialData に変換する
public ToonMaterialData unpackToonMaterial(ObjectData objData) {
    ToonMaterialData matData;
    matData.base_color = float4(asfloat(objData.data[0]), asfloat(objData.data[1]), asfloat(objData.data[2]), asfloat(objData.data[3]));
    matData.shade_color = float4(asfloat(objData.data[4]), asfloat(objData.data[5]), asfloat(objData.data[6]), asfloat(objData.data[7]));
    matData.outline_width = asfloat(objData.data[8]);
    matData.threshold = asfloat(objData.data[9]);
    matData.feather = asfloat(objData.data[10]);
    matData.normal_scale = asfloat(objData.data[11]);
    matData.albedo_texture_id = objData.data[12];
    matData.shade_texture_id = objData.data[13];
    matData.normal_texture_id = objData.data[14];
    matData.outline_color = float4(asfloat(objData.data[15]), asfloat(objData.data[16]), asfloat(objData.data[17]), asfloat(objData.data[18]));
    return matData;
}


