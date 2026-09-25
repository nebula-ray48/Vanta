// assets/shaders/tonemap.slang

struct PushConstants {
    uint hdr_texture_id;
    uint bloom_texture_id;
    float bloom_intensity;
    float ca_strength;
    float saturation;
    float contrast;
    float vignette_radius;
    float vignette_smoothness;
    float grain_amount;
    float _pad;
};
[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants;

[[vk::binding(1, 0)]] Texture2D global_textures[];
[[vk::binding(2, 0)]] SamplerState global_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// ==========================================
// 頂点シェーダー
// ==========================================
[shader("vertex")]
VSOutput vs_main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

// ==========================================
// ヘルパー関数（※関数の外に定義する）
// ==========================================
// ACES Filmic Tone Mapping
float3 tonemapACESFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// 疑似乱数生成 (フィルムグレイン用)
float random(float2 uv) {
    return frac(sin(dot(uv, float2(12.9898f, 78.233f))) * 43758.5453f);
}

// ==========================================
// フラグメントシェーダー (メイン処理)
// ==========================================
[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    // PushConstants から動的パラメータを取得
    float ca_strength = push_constants.ca_strength;
    float saturation = push_constants.saturation;
    float contrast = push_constants.contrast;
    float vignette_radius = push_constants.vignette_radius;
    float vignette_smoothness = push_constants.vignette_smoothness;
    float grain_amount = push_constants.grain_amount;

    // -------------------------------------------------------------
    // 1. 色収差付きで HDR テクスチャをサンプリング
    // -------------------------------------------------------------
    float2 center_dir = input.uv - float2(0.5f, 0.5f);
    float dist = length(center_dir);
    float2 ca_offset = center_dir * (dist * ca_strength);

    float r = global_textures[push_constants.hdr_texture_id].SampleLevel(global_sampler, input.uv + ca_offset, 0).r;
    float g = global_textures[push_constants.hdr_texture_id].SampleLevel(global_sampler, input.uv, 0).g;
    float b = global_textures[push_constants.hdr_texture_id].SampleLevel(global_sampler, input.uv - ca_offset, 0).b;
    float3 color = float3(r, g, b);

    // -------------------------------------------------------------
    // 2. ブルーム（横光条）を加算合成
    // -------------------------------------------------------------
    float3 bloomColor = global_textures[push_constants.bloom_texture_id].SampleLevel(global_sampler, input.uv, 0).rgb;
    color += bloomColor * push_constants.bloom_intensity;

    // -------------------------------------------------------------
    // 3. シネマティック・カラーグレーディング
    // -------------------------------------------------------------
    // 彩度
    float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(float3(luma), color, saturation);

    // コントラスト (0.18 のグレー基準)
    color = max(float3(0.0f), (color - 0.18f) * contrast + 0.18f);

    // スプリットトーニング (暗部にTeal、明部に暖色)
    float3 shadow_tint_mult = float3(0.96f, 0.99f, 1.04f); // わずかに青寄り
    float shadow_mask = 1.0f - saturate(luma * 2.0f);      // 暗い部分だけ抽出
    //color += lerp(shadow_tint, float3(0.03f, 0.01f, 0.0f), saturate(luma));
    color *= lerp(float3(1.0f), shadow_tint_mult, shadow_mask);

    // -------------------------------------------------------------
    // 4. ACES トーンマッピング
    // -------------------------------------------------------------
    float3 ldrColor = tonemapACESFilmic(color);

    // -------------------------------------------------------------
    // 5. ガンマ補正 (Linear -> sRGB)
    // -------------------------------------------------------------
    float3 srgbColor = pow(ldrColor, float3(1.0 / 2.2));

    // -------------------------------------------------------------
    // 6. 周辺減光 (Vignette) & フィルムグレイン (Film Grain)
    // -------------------------------------------------------------
    float vignette = smoothstep(vignette_radius, vignette_radius - vignette_smoothness, dist);
    srgbColor *= vignette;

    float noise = (random(input.uv) - 0.5f) * grain_amount;
    srgbColor += noise;

    return float4(saturate(srgbColor), 1.0f);
}
