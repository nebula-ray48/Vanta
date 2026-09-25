// lighting.slang
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

module common_lighting;

import common_types;
import common_bindings;

public static const float PI = 3.14159265359;

public float3 evaluateSphericalHarmonics(float3 N) {
    // L0
    float3 c = globalData.sh[0].rgb;
    // L1
    c += globalData.sh[1].rgb * N.y;
    c += globalData.sh[2].rgb * N.z;
    c += globalData.sh[3].rgb * N.x;
    // L2
    c += globalData.sh[4].rgb * (N.x * N.y);
    c += globalData.sh[5].rgb * (N.y * N.z);
    c += globalData.sh[6].rgb * (3.0 * N.z * N.z - 1.0);
    c += globalData.sh[7].rgb * (N.x * N.z);
    c += globalData.sh[8].rgb * (N.x * N.x - N.y * N.y);
    return max(c, float3(0.0));
}

public float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.000001);
}

public float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

public float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

public float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

public float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 法線マップからワールド空間法線を計算
public float3 getNormalFromMap(
    Texture2D normalMap,
    SamplerState texSampler,
    float2 uv,
    float3 worldPos,
    float3 worldNormal,
    float normalScale
) {
    float3 tangentNormal = normalMap.Sample(texSampler, uv).xyz * 2.0 - 1.0;
    tangentNormal = normalize(float3(tangentNormal.xy * normalScale, tangentNormal.z));

    float3 dPos1 = ddx(worldPos);
    float3 dPos2 = ddy(worldPos);
    float2 dUV1  = ddx(uv);
    float2 dUV2  = ddy(uv);
    float3 N = normalize(worldNormal);
    float3 T = normalize(dPos1 * dUV2.y - dPos2 * dUV1.y);
    float3 B = -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);
    return normalize(mul(tangentNormal, TBN));
}
