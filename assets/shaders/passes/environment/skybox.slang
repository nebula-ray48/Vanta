//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;

struct SkyboxVertexOutput {
    float4 position : SV_Position;
    float3 worldDir : WORLDDIR;
};

[shader("vertex")]
SkyboxVertexOutput skyboxVertexMain(uint vertexId : SV_VertexID) {
    SkyboxVertexOutput output;
    
    // Fullscreen triangle in NDC [-1, 1] at z = 1.0 (far plane in standard Vulkan [0, 1])
    float2 uv = float2((vertexId << 1) & 2, vertexId & 2);
    float4 clipPos = float4(uv * 2.0 - 1.0, 1.0, 1.0);
    output.position = clipPos;

    // Unproject to world direction
    float4 unprojected = mul(globalData.invViewProj, clipPos);
    output.worldDir = (unprojected.xyz / unprojected.w) - globalData.cameraPos;
    
    return output;
}

[shader("fragment")]
float4 skyboxFragmentMain(SkyboxVertexOutput input) : SV_Target {
    float3 dir = normalize(input.worldDir);
    float3 envColor = envCubemap.SampleLevel(dir, 0.0).rgb;

    envColor *= globalData.skyboxIntensity;

    // HDR Tone mapping (Reinhard) and Gamma correction
    // RenderGraph で HDR 出力とし、ポストプロセスでまとめてかけるためコメントアウト
    // envColor = envColor / (envColor + float3(1.0));
    // envColor = pow(envColor, float3(1.0 / 2.2));

    return float4(envColor, 1.0);
}
