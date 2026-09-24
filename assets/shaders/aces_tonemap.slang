// Krzysztof Narkowicz氏によるACES Filmic Tone Mapping近似式
// HDR（Linear）のカラー値を、ディスプレイ向けのSDR（LDR）カラー値に変換します
float3 tonemapACESFilmic(float3 x)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;

    // 入力値をディスプレイの輝度レンジにマッピング
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// テクスチャバインディング
Texture2D<float4> gHDRSceneTex; // レンダリング直後のHDR（Linear）カラーバッファ
SamplerState      gPointSampler;

[shader("pixel")]
void main(
    float2 uv : TEXCOORD0,
    out float4 outColor : SV_Target)
{
    // 1. HDRリニア空間のカラーを取得
    float3 hdrColor = gHDRSceneTex.SampleLevel(gPointSampler, uv, 0).rgb;

    // （オプショナル）もしSSAOなどをここで乗算する場合はここで計算
    // hdrColor *= ssaoValue;

    // 2. ACES Filmic トーンマッピングを適用
    float3 ldrColor = tonemapACESFilmic(hdrColor);

    // 3. ガンマ補正 (Linear -> sRGB)
    // ディスプレイ表示用に1/2.2乗（または正確なsRGB変換）を適用します
    float3 srgbColor = pow(ldrColor, float3(1.0 / 2.2));

    // 4. 最終出力
    outColor = float4(srgbColor, 1.0);
}
