//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

/**
 * toon_outline.slang
 *
 * Toon シェーディングのアウトライン（輪郭線）描画シェーダー。
 * 反転法線拡大法（Inverted Hull / Backface Method）を実装しています。
 *
 * --- アルゴリズムの概要 ---
 * 1. 頂点シェーダー: 通常描画（toon_main.slang）より少し大きなメッシュを生成する
 *    - 各頂点をワールド法線方向に outline_width * 0.002 だけ押し出す
 * 2. パイプライン設定: 前面（Front Face）をカリングし、裏面だけを描画する
 *    - 正面からは大きくなった裏面がメッシュの縁に沿って見える＝アウトライン
 * 3. フラグメントシェーダー: ToonMaterialData.outline_color を出力するだけ
 *
 * --- 描画順序 ---
 * vulkan_renderer_draw.cpp の MainColorPass 内:
 *   1. toon_pipeline_     → 本体（正面カリング、通常テクスチャ）
 *   2. toon_outline_pipeline_ → アウトライン（前面カリング、1色塗りつぶし）
 *
 * --- パイプライン設定場所 ---
 * vulkan_renderer_init.cpp の initialize_pipeline_resources() 内:
 *   .with_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE)
 *   .with_depth_test(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS_OR_EQUAL)
 *   ↑深度書き込みなし（深度バッファ上はアウトラインが存在しないことにする）
 *
 * --- 太さを調整するには ---
 * ToonMaterialParams::outline_width (render_types.h) を変更するか、
 * ImGui の「Outline Width」スライダーを使う（tests/test_main.cpp）
 */

import common_types;
import common_bindings;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID) {
    VertexOutput output;

    ObjectData objData = objectBuffer[instance_id];
    float4x4 model = objData.model_matrix;
    ToonMaterialData matData = unpackToonMaterial(objData);

    float4 localPos = float4(input.position, 1.0);
    
    float3 normalWorld = normalize(mul((float3x3)model, input.normalDir));
    float4 worldPos = mul(model, localPos);
    
    // Outline thickness
    float thickness = matData.outline_width * 0.002;
    worldPos.xyz += normalWorld * thickness;

    output.position = mul(globalData.viewProj, worldPos);
    output.worldPos = worldPos.xyz;
    output.color = input.color;
    output.normalDir = normalWorld;
    output.uv = input.uv;
    output.instanceId = instance_id;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    ObjectData objData = objectBuffer[input.instanceId];
    ToonMaterialData matData = unpackToonMaterial(objData);
    
    return float4(matData.outline_color.rgb, 1.0);
}
