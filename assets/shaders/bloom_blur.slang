// assets/shaders/bloom_blur.slang

struct PushConstants {
    uint input_texture_id;
    float streak_length;    // ストリークの広がり幅 (例: 3.5f)
    float texel_size_x;     // 1.0 / 画面幅
    float texel_size_y;     // 1.0 / 画面高さ
    float tint_r;           // シネマティックな着色 R
    float tint_g;           // シネマティックな着色 G
    float tint_b;           // シネマティックな着色 B
    float intensity;        // ブルーム強度 (例: 1.0f)
};
[[vk::push_constant]] ConstantBuffer<PushConstants> push_constants;

[[vk::binding(1, 0)]] Texture2D global_textures[];
[[vk::binding(2, 0)]] SamplerState global_sampler;

struct VSOutput {
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
VSOutput vs_main(uint vertexID : SV_VertexID) {
    VSOutput output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * 2.0f - 1.0f, 0.0f, 1.0f);
    return output;
}

[shader("fragment")]
float4 fs_main(VSOutput input) : SV_Target {
    float3 result = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;

    // アナモルフィック・レンズ特有の広範囲な水平ストリーク (33タップの指数減衰サンプリング)
    const int SAMPLES = 16;
    for (int i = -SAMPLES; i <= SAMPLES; ++i) {
        float weight = exp(-abs(float(i)) * 0.16f);
        
        float2 offset = float2(float(i) * push_constants.streak_length * push_constants.texel_size_x, 0.0f);
        float3 sampleColor = global_textures[push_constants.input_texture_id].SampleLevel(global_sampler, input.uv + offset, 0).rgb;
        
        result += sampleColor * weight;
        totalWeight += weight;
    }

    result /= max(totalWeight, 1e-4f);

    float3 tint = float3(push_constants.tint_r, push_constants.tint_g, push_constants.tint_b);
    result *= tint * push_constants.intensity;

    return float4(result, 1.0f);
}
