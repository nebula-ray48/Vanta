//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;
import common_lighting;

// =========================================================
// 1. Toon ライティング金型関数群
// =========================================================

/**
 * ① ディフューズ（明暗の階調比率 0.0=影, 1.0=日向）
 */
float calculate_toon_diffuse(float3 N, float3 L, float threshold, float feather) {
    float NdotL = dot(N, L);
    float half_lambert = NdotL * 0.5 + 0.5; // ハーフランバートで自然な階調を作る
    return smoothstep(threshold - feather, threshold + feather, half_lambert);
}

/**
 * セルアニメ調の2段影（1影・2影）を計算する関数
 * 
 * @param albedo          : ベースのテクスチャカラー
 * @param shade_color     : 影の色合い（青紫など）
 * @param sun_color       : 太陽光の色と強さ
 * @param ramp1           : 1影の明暗比率 (0.0=影, 1.0=日向)
 * @param ramp2           : 2影の明暗比率 (0.0=濃い影, 1.0=浅い影)
 * @return 陰影合成後のサーフェスカラー
 */
float3 calculate_two_step_shadow(
    float3 albedo,
    float3 shade_tint,
    float3 sun_color,
    float  ramp1,   // 1影の明暗比率 (0.0=影, 1.0=日向)
    float  ramp2    // 2影の明暗比率 (0.0=濃い影, 1.0=浅い影)
) {
    // 1影（浅い影：青紫の色合いを綺麗に残す）
    float3 shade1 = albedo * shade_tint * 1.8;
    // 2影（深い落ち影：さらに暗く引き締める）
    float3 shade2 = albedo * shade_tint * 0.8;

    // 2段階ブレンド (2影 ↔ 1影 ↔ 日向)
    float3 shadow = lerp(shade2, shade1, ramp2);
    return lerp(shadow, albedo * sun_color, ramp1);
}

/**
 * ③ セル調リムライト（輪郭の光）
 */
float calculate_toon_rim(float3 N, float3 V, float threshold, float feather) {
    float NdotV = saturate(dot(N, V));
    float rim = 1.0 - NdotV;
    return smoothstep(threshold - feather, threshold + feather, rim);
}

/**
 * ④ セル調スペキュラ（ハイライト）
 */
float calculate_toon_specular(float3 N, float3 L, float3 V, float threshold, float feather) {
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    return smoothstep(threshold - feather, threshold + feather, NdotH);
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
    // ---------------------------------------------------------
    // 1. データの準備
    // ---------------------------------------------------------
    ObjectData objData = objectBuffer[input.instanceId];
    ToonMaterialData matData = unpackToonMaterial(objData);

    float3 N = normalize(input.normalDir);
    if (matData.normal_texture_id != 0) {
        uint normalID = NonUniformResourceIndex(matData.normal_texture_id);
        N = getNormalFromMap(bindlessTextures[normalID], defaultSampler, input.uv, input.worldPos, N, matData.normal_scale);
    }

    float3 L = normalize(globalData.sunDirection.xyz);
    float3 V = normalize(globalData.cameraPos - input.worldPos);

    // ---------------------------------------------------------
    // 2. テクスチャカラー（アルベド）の取得
    // ---------------------------------------------------------
    float3 albedo = matData.base_color.rgb * input.color;
    if (matData.albedo_texture_id != 0) {
        uint texID = NonUniformResourceIndex(matData.albedo_texture_id);
        albedo *= bindlessTextures[texID].Sample(defaultSampler, input.uv).rgb;
    }

    // ---------------------------------------------------------
    // 3. ライティング計算（2段影・環境光・リムライト）
    // ---------------------------------------------------------
    // ① 2段影（1影: matData.threshold, 2影: その半分の深さ）
    float ramp1 = calculate_toon_diffuse(N, L, matData.threshold, matData.feather);
    float ramp2 = calculate_toon_diffuse(N, L, matData.threshold * 0.18, matData.feather);
    float3 sun_color = globalData.sunColor.rgb * 0.9;
    float3 diffuse = calculate_two_step_shadow(albedo, matData.shade_color.rgb, sun_color, ramp1, ramp2);

    // ② 環境光（影が真っ暗になるのを防止）
    float3 irradiance = evaluateSphericalHarmonics(N);
    float3 ambient = irradiance * albedo * 0.12 * globalData.iblIntensity;

    // ③ 青〜紫グラデーションのリムライト（輪郭光）
    float rim_ramp = calculate_toon_rim(N, V, 0.65, 0.15);
    float3 cyan_blue = float3(0.1, 0.6, 1.0);  // 青
    float3 purple    = float3(0.8, 0.2, 1.0);  // 紫
    float3 rim_color = lerp(cyan_blue, purple, rim_ramp);
    float3 rim_light = rim_ramp * rim_color * 1.8;

    // ---------------------------------------------------------
    // 4. 最終合成
    // ---------------------------------------------------------
    float3 final_color = diffuse + ambient + rim_light;

    return float4(final_color, matData.base_color.a);
}
