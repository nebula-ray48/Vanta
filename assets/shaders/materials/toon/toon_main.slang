//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;
import common_lighting;

// =========================================================
// 1. Toon ライティング関数（金型）
// =========================================================

/**
 * トゥーン調のディフューズ（明暗の階調）を計算
 */
float calculate_toon_diffuse(float3 N, float3 L, float threshold, float feather) {
    float NdotL = dot(N, L);
    float half_lambert = NdotL * 0.5 + 0.5; // ハーフランバートで自然な階調を作る
    return smoothstep(threshold - feather, threshold + feather, half_lambert);
}

/**
 * セル調リムライト（輪郭の光）を計算
 */
float calculate_toon_rim(float3 N, float3 V, float threshold, float feather) {
    float NdotV = saturate(dot(N, V));
    float rim = 1.0 - NdotV;
    return smoothstep(threshold - feather, threshold + feather, rim);
}

// =========================================================
// 2. 頂点シェーダー (Vertex Shader)
// =========================================================
[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID) {
    VertexOutput output;

    float4x4 model = objectBuffer[instance_id].model_matrix;
    float4 worldPos = mul(model, float4(input.position, 1.0));

    output.position = mul(globalData.viewProj, worldPos);
    output.worldPos = worldPos.xyz;

    output.color = input.color;
    output.normalDir = normalize(mul((float3x3)model, input.normalDir));

    output.uv = input.uv;
    output.instanceId = instance_id;

    return output;
}

// =========================================================
// 3. フラグメントシェーダー (Fragment Shader)
// =========================================================
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    ObjectData objData = objectBuffer[input.instanceId];
    ToonMaterialData matData = unpackToonMaterial(objData);

    // 1. ベクトルの準備
    float3 N = normalize(input.normalDir);

    // ノーマルマップの適用（パーツの溝や凹凸を法線に反映）
    if (matData.normal_texture_id != 0) {
        uint normalID = NonUniformResourceIndex(matData.normal_texture_id);
        N = getNormalFromMap(
            bindlessTextures[normalID],
            defaultSampler,
            input.uv,
            input.worldPos,
            N,
            matData.normal_scale
        );
    }

    float3 L = normalize(globalData.sunDirection.xyz);
    float3 V = normalize(globalData.cameraPos - input.worldPos);

    // 2. テクスチャカラー（アルベド）の取得
    float3 albedo = matData.base_color.rgb * input.color;
    if (matData.albedo_texture_id != 0) {
        uint texID = NonUniformResourceIndex(matData.albedo_texture_id);
        albedo *= bindlessTextures[texID].Sample(defaultSampler, input.uv).rgb;
    }

    // 影色（テクスチャ色 × 設定された影の色合い）
    float3 shadow_base = albedo * matData.shade_color.rgb;

    // 3. Toon 明暗の計算 (Diffse Ramp)
    float ramp = calculate_toon_diffuse(N, L, matData.threshold, matData.feather);

    // 太陽光の色（1.0前後の自然な明るさにスケール）
    float3 sun_light = globalData.sunColor.rgb * 0.9;

    // 日向色と影色のブレンド（コントラストをしっかり出す）
    float3 diffuse = lerp(shadow_base * 0.6, albedo * sun_light, ramp);

    // 4. 環境光（アンビエント）
    // 影の部分が真っ黒にならない程度に控えめ（0.12）に足す
    float3 irradiance = evaluateSphericalHarmonics(N);
    float3 ambient = irradiance * albedo * 0.12 * globalData.iblIntensity;

    // 5. リムライト（エッジの薄い光）
    float rim_ramp = calculate_toon_rim(N, V, 0.75, 0.05);
    float3 rim_light = rim_ramp * globalData.sunColor.rgb * 0.25 * ramp; // 日向の輪郭にのみ乗せる

    // 最終合成（過剰な発光を防ぐため saturate で適正範囲に収める）
    float3 final_color = diffuse + ambient + rim_light;

    return float4(final_color, matData.base_color.a);
}

