//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

// ==========================================
// 1. データ構造の定義
// ==========================================
struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color : COLOR;
    [[vk::location(2)]] float3 normalDir : NORMAL;
    [[vk::location(3)]] float2 uv : TEXCOORD;
    [[vk::location(4)]] uint textureId : TEXINDEX;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 color : COLOR;
    float3 normalDir : NORMAL;
    float2 uv : TEXCOORD;
    nointerpolation uint textureId : TEXINDEX;
};

// TODO　将来の拡張用
struct Transform {
    float3 position;
    float4 rotation;
    float3 scale;
};
struct Entity {
    uint32_t id;
    uint32_t mesh_id;
    Transform transform;
};

// ==========================================
// 2. 外部からのデータ受け入れ口
// ==========================================

// --- Set 0: グローバルデータ ---
struct GlobalUbo {
    float4x4 viewProj;
    float3 cameraPos;
    float _padding;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> globalData;


// --- Set 0: Bindless リソース ---
// サイズ不定のテクスチャ配列 (Set 0, Binding 1)
[[vk::binding(1, 0)]]
Texture2D bindlessTextures[];

// 共通サンプラー (Set 0, Binding 2)
[[vk::binding(2, 0)]]
SamplerState defaultSampler;


// TODO: 将来的に PushConstants のモデル行列を廃止し、
// ここに StructuredBuffer<Entity> entities; を置いて
// SV_InstanceID で一括フェッチするのが理想

struct PushConstants {
    float4x4 model;
};

[[vk::push_constant]]
ConstantBuffer<PushConstants> push;


// ==========================================
// 3. 処理ロジック
// ==========================================

[shader("vertex")]
VertexOutput vertexMain(VertexInput input) {
    VertexOutput output;

    float4 worldPos = mul(push.model, float4(input.position, 1.0));

    output.position = mul(globalData.viewProj, worldPos);

    output.color = input.color;
    output.normalDir = input.normalDir;

    output.uv = input.uv;
    output.textureId = input.textureId;

    return output;
}
/*
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {

    // 1. ライティング計算
    float3 N = normalize(input.normalDir);
    float3 lightDir = normalize(float3(1.0, 1.0, 1.0));
    float diffuse = max(0.0, dot(N, lightDir));
    float ambient = 0.2;
    float lightIntensity = ambient + diffuse;

    uint texID = NonUniformResourceIndex(input.textureId);
    float4 texColor = bindlessTextures[texID].Sample(defaultSampler, input.uv);

    // 3. ベースカラー × テクスチャ色 × 光の強さ
    float3 finalColor = input.color * texColor.rgb * lightIntensity;

    return float4(finalColor, 1.0);
}
*/

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    // Simple directional light
    float3 N = normalize(input.normalDir);
    float3 lightDir = normalize(float3(0.5, 1.0, 0.5));
    float intensity = max(0.2, dot(N, lightDir));
    return float4(input.color * intensity, 1.0);
}
