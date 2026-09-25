// assets/shaders/bloom_extract.slang

struct PushConstants {
    uint hdr_texture_id;
    float threshold;      // 例: 1.0f (1.0以上の光を抽出)
    float soft_knee;      // 例: 0.5f (滑らかに立ち上げる幅)
};
[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants;

[[vk::binding(1, 0)]] Texture2D global_textures[];
[[vk::binding(2, 0)]] SamplerState global_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// tonemap.slang と同様のフルスクリーン三角形
[shader("vertex")]
VSOutput vs_main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

// 輝度計算 (Rec.709 Linear)
float getLuminance(float3 color) {
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    float3 color = global_textures[push_constants.hdr_texture_id].SampleLevel(global_sampler, input.uv, 0).rgb;

    float luma = getLuminance(color);
    float threshold = push_constants.threshold;
    float knee = threshold * push_constants.soft_knee;

    // Soft-knee カーブによる滑らかな高輝度抽出 (Karis prefilter)
    float soft = luma - threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = (soft * soft) / (4.0f * max(knee, 1e-4f));

    float contribution = max(soft, luma - threshold);
    contribution /= max(luma, 1e-4f);

    float3 extracted = color * max(contribution, 0.0f);
    return float4(extracted, 1.0f);
}
