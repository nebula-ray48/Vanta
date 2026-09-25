// utils.slang
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

module common_utils;

// ACES Filmic Tone Mapping
public float3 tonemapACESFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 疑似乱数生成 (フィルムグレイン用)
public float random(float2 uv) {
    return frac(sin(dot(uv, float2(12.9898f, 78.233f))) * 43758.5453f);
}
