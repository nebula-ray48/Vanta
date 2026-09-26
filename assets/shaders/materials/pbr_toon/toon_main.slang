//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;
import common_lighting;

// Toon 用のディフューズ（拡散光）を計算する関数
float calculate_toon_diffuse(float3 N, float3 L, float threshold, float feather) {
    float NdotL = saturate(dot(N, L));
    return smoothstep(threshold - feather, threshold + feather, NdotL);
}

/**
 * 2. セル調リムライト（輪郭の光）を計算
 * @param N         : 表面の法線ベクトル
 * @param V         : カメラへの視線ベクトル
 * @param threshold : リムライトが出る角度のしきい値
 * @param feather   : リムライトの境界のボケ幅
 * @return          : リムライトの強さ (0.0 ~ 1.0)
 */
 float calculate_toon_rim(float3 N, float3 V, float threshold, float feather) {
    // 視線と法線の内積（正面ほど 1.0、端っこ・輪郭ほど 0.0 になる）
    float NdotV = saturate(dot(N, V));
    
    // 輪郭部分（端）を 1.0 に反転
    float rim = 1.0 - NdotV;
    return smoothstep(threshold - feather, threshold + feather, rim);
}

/**
 * 3. セル調ハイライト（天使の輪・金属のテカり）を計算
 * @param N         : 表面の法線ベクトル
 * @param L         : 光源への方向ベクトル
 * @param V         : カメラへの視線ベクトル
 * @param roughness : 表面の粗さ（ハイライトの広がり）
 * @return          : ハイライトの強さ (0.0 ~ 1.0)
 */
float calculate_toon_specular(float3 N, float3 L, float3 V, float threshold, float feather) {
    // ハーフベクトル (光と視線の真ん中のベクトル)
    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    // ステップ状にカットしてアニメ調のハイライトにする
    return smoothstep(threshold - feather, threshold + feather, NdotH);
}

/**
 * 4. 全てを統合して最終的なピクセルカラーを決定するメイン関数
 */
float3 evaluate_toon_shading(
    float3 albedo,          // ベースの色（テクスチャなど）
    float3 shade_color,      // 影の色
    float3 light_color,      // 太陽光の色と強さ
    float3 N, float3 L, float3 V,
    float  diffuse_threshold,
    float  diffuse_feather
) {
    // ① 明暗比率を計算 (0.0 = 完全な影, 1.0 = 日向)
    float ramp = calculate_toon_diffuse(N, L, diffuse_threshold, diffuse_feather);
    // ② 影色とベース色をブレンド
    float3 surface_color = lerp(shade_color, albedo, ramp);
    // ③ 光の色を掛けて最終色にする
    return surface_color * light_color;
}

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
// 3. フラグメントシェーダー（ピクセルごとの色決め）
// =========================================================
[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    // ---------------------------------------------------------
    // 1. データの準備
    // ---------------------------------------------------------
    ObjectData objData = objectBuffer[input.instanceId];
    
    // マテリアルパラメータ（Toon用）の取り出し
    float4 base_color   = float4(asfloat(objData.data[0]), asfloat(objData.data[1]), asfloat(objData.data[2]), asfloat(objData.data[3]));
    float4 shade_color  = float4(asfloat(objData.data[4]), asfloat(objData.data[5]), asfloat(objData.data[6]), asfloat(objData.data[7]));
    float  threshold    = asfloat(objData.data[9]);  // 影のしきい値 (例: 0.5)
    float  feather      = asfloat(objData.data[10]); // ボケ幅 (例: 0.02)
    uint   albedo_tex   = objData.data[11];

    // ベクトルの正規化
    float3 N = normalize(input.normalDir);
    float3 L = normalize(globalData.sunDirection.xyz);
    float3 V = normalize(globalData.cameraPos - input.worldPos);

    // ---------------------------------------------------------
    // 2. テクスチャカラーの取得
    // ---------------------------------------------------------
    float3 albedo = base_color.rgb * input.color;
    if (albedo_tex != 0) {
        uint texID = NonUniformResourceIndex(albedo_tex);
        albedo *= bindlessTextures[texID].Sample(defaultSampler, input.uv).rgb;
    }

    // 影色にもテクスチャの色を反映（ベースのテクスチャ色 × 指定した影の色合い）
    float3 shadow_base = albedo * shade_color.rgb;

    // ---------------------------------------------------------
    // 3. セル陰影の計算 (Toon Diffuse)
    // ---------------------------------------------------------
    // ① 金型関数を呼んで 0.0(影) 〜 1.0(日向) の比率を計算
    float ramp = calculate_toon_diffuse(N, L, threshold, feather);

    // ② 太陽光の色と強さ
    float3 sun_light = globalData.sunColor.rgb * globalData.sunDirection.w;

    // ③ 影色と日向色をブレンド
    float3 diffuse = lerp(shadow_base, albedo * sun_light, ramp);

    // ---------------------------------------------------------
    // 4. 環境光（アンビエント）の足し合わせ
    // ---------------------------------------------------------
    // 球面調和（SH）から環境の光を少し拾って足すことで、真っ黒になるのを防ぐ
    float3 ambient = evaluateSphericalHarmonics(N) * albedo * 0.2 * globalData.iblIntensity;

    float3 final_color = diffuse + ambient;

    return float4(final_color, base_color.a);
}
