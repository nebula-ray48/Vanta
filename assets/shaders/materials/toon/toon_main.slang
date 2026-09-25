//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;
import common_lighting;

// =========================================================
// 1. Toon ライティング関数（セルルック Diffuse 計算）
// =========================================================
float calculate_toon_diffuse(float3 N, float3 L, float threshold, float feather) {
    float NdotL = saturate(dot(N, L));
    return smoothstep(threshold - feather, threshold + feather, NdotL);
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
    PbrMaterialData matData = unpackPbrMaterial(objData);

    // テクスチャからベースカラー（アルベド）を取得
    float3 baseColor = matData.base_color.rgb * input.color;
    if (matData.albedo_texture_id != 0) {
        uint texID = NonUniformResourceIndex(matData.albedo_texture_id);
        baseColor *= bindlessTextures[texID].Sample(defaultSampler, input.uv).rgb;
    }

    // 法線(N) と 太陽光ベクトル(L)
    float3 N = normalize(input.normalDir);
    float3 L = normalize(globalData.sunDirection.xyz);

    // --- Toon Diffuse (N·L Ramp) ---
    float ramp = calculate_toon_diffuse(N, L, 0.4, 0.02);

    float3 lightColor = baseColor * globalData.sunColor.rgb * globalData.sunDirection.w;
    float3 shadowColor = baseColor * float3(0.5, 0.5, 0.7); // 影色（青みをブレンドしたセル影）

    float3 finalDiffuse = lerp(shadowColor, lightColor, ramp);

    return float4(finalDiffuse, matData.base_color.a);
}
