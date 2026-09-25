//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;
import common_lighting;


// ==========================================
// 4. 処理ロジック
// ==========================================

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID) {
    VertexOutput output;

    float4x4 model = objectBuffer[instance_id].model_matrix;
    float4 worldPos = mul(model, float4(input.position, 1.0));

    output.position = mul(globalData.viewProj, worldPos);
    output.worldPos = worldPos.xyz;

    output.color = input.color;
    // FIXME: Non-uniform scale normal matrix
    output.normalDir = normalize(mul((float3x3)model, input.normalDir));

    output.uv = input.uv;
    output.instanceId = instance_id;

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    ObjectData objData = objectBuffer[input.instanceId];
    PbrMaterialData matData = unpackPbrMaterial(objData);

    // マテリアルパラメータの取得
    float3 albedo = matData.base_color.rgb * input.color;

    if (matData.albedo_texture_id != 0) {
        uint texID = NonUniformResourceIndex(matData.albedo_texture_id);
        float4 texColor = bindlessTextures[texID].Sample(defaultSampler, input.uv);
        albedo *= texColor.rgb;
    }

    float metallic = matData.metallic;
    float roughness = matData.roughness;

    // MRMテクスチャの適用 (glTF標準: G = Roughness, B = Metallic)
    if (matData.mrm_texture_id != 0) {
        uint mrmID = NonUniformResourceIndex(matData.mrm_texture_id);
        float4 mrmColor = bindlessTextures[mrmID].Sample(defaultSampler, input.uv);
        roughness *= mrmColor.g;
        metallic  *= mrmColor.b;
    }

    float3 N = normalize(input.normalDir);

    // ノーマルマップの適用 (法線の凹凸強度 normal_scale を反映)
    if (matData.normal_texture_id != 0) {
        uint normalID = NonUniformResourceIndex(matData.normal_texture_id);
        N = getNormalFromMap(
            bindlessTextures[normalID], // Bindlessからテクスチャを渡す
            defaultSampler,
            input.uv,
            input.worldPos,
            N,                          // ベースとなる頂点法線
            matData.normal_scale
        );
    }

    float3 V = normalize(globalData.cameraPos - input.worldPos);
    float NdotV = max(dot(N, V), 0.0);

    // 反射率の初期値 (F0)
    float3 F0 = float3(0.04);
    F0 = lerp(F0, albedo, metallic);

    // ---------------------------------------------------------
    // 1. 直接光 (Directional Sun Light)
    // ---------------------------------------------------------
    float3 L = normalize(globalData.sunDirection.xyz);
    float3 H = normalize(V + L);
    float3 radiance = globalData.sunColor.rgb * globalData.sunDirection.w;

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator    = NDF * G * F;
    float denominator = 4.0 * NdotV * max(dot(N, L), 0.0) + 0.0001;
    float3 specularDirect = numerator / denominator;

    float3 kS_direct = F;
    float3 kD_direct = (float3(1.0) - kS_direct) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    float3 directLight = (kD_direct * albedo / PI + specularDirect) * radiance * NdotL;

    // --- Shadow Mapping (PCF) ---
    float shadow = 1.0;
    if (globalData.shadowMapIndex != 0) {
        float4 lightSpacePos = mul(globalData.lightViewProj, float4(input.worldPos, 1.0));
        float3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
        // Transform from [-1, 1] to [0, 1] for UVs, Vulkan Z is [0, 1]
        projCoords.x = projCoords.x * 0.5 + 0.5;
        projCoords.y = projCoords.y * 0.5 + 0.5;

        if (projCoords.z >= 0.0 && projCoords.z <= 1.0 &&
            projCoords.x >= 0.0 && projCoords.x <= 1.0 &&
            projCoords.y >= 0.0 && projCoords.y <= 1.0) {

            uint shadowTexID = NonUniformResourceIndex(globalData.shadowMapIndex);
            float currentDepth = projCoords.z;
            float bias = max(0.005 * (1.0 - dot(N, L)), 0.001);

            float shadowSum = 0.0;
            // 3x3 PCF
            float2 texelSize = 1.0 / 2048.0;
            for (int x = -1; x <= 1; ++x) {
                for (int y = -1; y <= 1; ++y) {
                    float pcfDepth = bindlessTextures[shadowTexID].SampleLevel(defaultSampler, projCoords.xy + float2(x, y) * texelSize, 0.0).r;
                    shadowSum += currentDepth - bias > pcfDepth ? 0.0 : 1.0;
                }
            }
            shadow = shadowSum / 9.0;
        }
    }

    directLight *= shadow;

    // ---------------------------------------------------------
    // 2. 環境光 (Image-Based Lighting / IBL)
    // ---------------------------------------------------------
    // (A) Diffuse Irradiance (Spherical Harmonics)
    float3 F_ambient = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kS_ambient = F_ambient;
    float3 kD_ambient = (float3(1.0) - kS_ambient) * (1.0 - metallic);

    float3 irradiance = evaluateSphericalHarmonics(N);
    float3 diffuseIBL = irradiance * albedo * kD_ambient * globalData.iblIntensity;

    // (B) Specular Radiance (Prefiltered Cubemap + BRDF LUT)
    float3 R = reflect(-V, N);
    float lod = roughness * globalData.maxReflectionLod;
    float3 prefilteredColor = envCubemap.SampleLevel(R, lod).rgb;

    uint brdfLutID = NonUniformResourceIndex(globalData.brdfLutIndex);
    float2 brdf = bindlessTextures[brdfLutID].Sample(defaultSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y) * globalData.iblIntensity;

    // --- AO (Ambient Occlusion) ---
    float ao = 1.0;
    if (matData.occlusion_texture_id != 0) {
        uint aoID = NonUniformResourceIndex(matData.occlusion_texture_id);
        float sampledAO = bindlessTextures[aoID].Sample(defaultSampler, input.uv).r;
        ao = 1.0 + matData.occlusion_strength * (sampledAO - 1.0);
    }
    
    // SSAOを反映
    if (globalData.ssaoMapIndex != 0) {
        uint ssaoID = NonUniformResourceIndex(globalData.ssaoMapIndex);
        float2 screenUV = input.position.xy / globalData.screenSize;
        float screenSSAO = bindlessTextures[ssaoID].SampleLevel(defaultSampler, screenUV, 0.0).r;
        ao *= screenSSAO;
    }

    float3 ambient = (diffuseIBL + specularIBL) * ao;

    // ---------------------------------------------------------
    // 3. エミッシブ (Emissive)
    // ---------------------------------------------------------
    float3 emissive = float3(0.0);
    if (matData.emissive_texture_id != 0) {
        uint emissiveID = NonUniformResourceIndex(matData.emissive_texture_id);
        emissive = bindlessTextures[emissiveID].Sample(defaultSampler, input.uv).rgb;
    }

    // ---------------------------------------------------------
    // 4. 最終合成 & トーンマッピング
    // ---------------------------------------------------------
    float3 color = directLight + ambient + emissive;

    // ACES Filmic トーンマッピング と ガンマ補正 (Linear -> sRGB)
    // RenderGraph で HDR 出力とし、ポストプロセスでまとめてかけるためコメントアウト
    // color = tonemapACESFilmic(color);
    // color = pow(color, float3(1.0 / 2.2));

    return float4(color, matData.base_color.a);
}
