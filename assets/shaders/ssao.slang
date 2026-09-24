//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

// Vulkan環境向けのリソースバインディング
[[vk::binding(0, 0)]] Texture2D<float>   gDepthTex;       // 深度マップ
[[vk::binding(1, 0)]] Texture2D<float3>  gNormalTex;      // 法線マップ (ビュー空間)
[[vk::binding(2, 0)]] Texture2D<float3>  gNoiseTex;       // ランダム回転用のノイズマップ
[[vk::binding(3, 0)]] RWTexture2D<float> gOutputTex;      // SSAO計算結果の出力先

[[vk::binding(4, 0)]] SamplerState       gPointSampler;

// ユニフォームバッファ（std140レイアウトを考慮したパッキング）
struct SsaoUniforms
{
    float4x4 projMatrix;
    float4x4 invProjMatrix;
    float4   samples[64]; // Data-Oriented: float3は16バイトアライメントの都合上float4で定義する
    float2   screenSize;
    float    radius;
    float    bias;
};

[[vk::binding(5, 0)]] ConstantBuffer<SsaoUniforms> ubo;

// 画面座標からビュー空間の位置を再構築する関数
float3 getPositionVS(float2 uv)
{
    float depth = gDepthTex.SampleLevel(gPointSampler, uv, 0).r;

    // Vulkanのクリップ空間 (Z: 0.0 ~ 1.0)
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);

    float4 viewPos = mul(ubo.invProjMatrix, clipPos);
    return viewPos.xyz / viewPos.w;
}

[shader("compute")]
[numthreads(16, 16, 1)]
void main(uint3 threadId : SV_DispatchThreadID)
{
    uint2 pixelPos = threadId.xy;
    if (pixelPos.x >= (uint)ubo.screenSize.x || pixelPos.y >= (uint)ubo.screenSize.y) return;

    float2 uv = (float2(pixelPos) + 0.5) / ubo.screenSize;

    // 現在のピクセルのビュー空間位置と法線を取得
    float3 originVS = getPositionVS(uv);
    float3 normalVS = normalize(gNormalTex.SampleLevel(gPointSampler, uv, 0).xyz);

    // ノイズテクスチャをタイル状に配置してランダムベクトルを取得
    float2 noiseUV = ubo.screenSize * uv / 4.0; 
    float3 randomVec = normalize(gNoiseTex.SampleLevel(gPointSampler, noiseUV, 0).xyz);

    // 法線とランダムベクトルから接空間（TBN行列）を作成
    float3 tangent = normalize(randomVec - normalVS * dot(randomVec, normalVS));
    float3 bitangent = cross(normalVS, tangent);
    float3x3 tbn = float3x3(tangent, bitangent, normalVS);

    float occlusion = 0.0;
    const int sampleCount = 64;

    // 周辺サンプリングループ
    for (int i = 0; i < sampleCount; ++i)
    {
        // サンプルを接空間からビュー空間へ変換 (float4のxyz成分を抽出)
        float3 samplePosVS = mul(tbn, ubo.samples[i].xyz);

        // 現在の位置から半径を考慮したサンプル位置を計算
        samplePosVS = originVS + samplePosVS * ubo.radius;

        // ビュー空間の位置をスクリーン（UV）空間へ射影
        float4 offset = float4(samplePosVS, 1.0);
        offset = mul(ubo.projMatrix, offset);
        offset.xyz /= offset.w;
        
        // クリップ空間からUV座標へ変換
        offset.xy = offset.xy * 0.5 + 0.5;

        // サンプル位置の実際の深度を取得
        float sampleDepthVS = getPositionVS(offset.xy).z;

        // 遮蔽の判定
        // ※ビュー空間が右手系（カメラの奥がマイナスZ）の場合、Zの値が大きいほど手前になる
        float rangeCheck = smoothstep(0.0, 1.0, ubo.radius / abs(originVS.z - sampleDepthVS));
        if (sampleDepthVS >= samplePosVS.z + ubo.bias)
        {
            occlusion += 1.0 * rangeCheck;
        }
    }

    // 遮蔽度を反転して「可視性（Visibility）」として正規化
    float accessibility = 1.0 - (occlusion / float(sampleCount));

    // 結果を出力テクスチャへ書き込む
    gOutputTex[pixelPos] = accessibility;
}