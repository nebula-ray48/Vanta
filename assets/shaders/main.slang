//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

// ==========================================
// 1. データ構造の定義
// ==========================================
struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color : COLOR;
    [[vk::location(2)]] float3 normalDir : NORMAL;
    [[vk::location(3)]] float2 uv : TEXCOORD;
    [[vk::location(4)]] uint textureId : TEXINDEX;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : WORLDPOS;
    float3 color : COLOR;
    float3 normalDir : NORMAL;
    float2 uv : TEXCOORD;
    nointerpolation uint instanceId : INSTANCEID;
};

// ==========================================
// 2. 外部からのデータ受け入れ口
// ==========================================

// --- Set 0: グローバルデータ ---
struct GlobalUbo {
    float4x4 viewProj;
    float4x4 invViewProj;
    float4x4 lightViewProj;
    float3 cameraPos;
    float _padding;
    float4 sunDirection; // w = intensity
    float4 sunColor;
    float4 ambientColor;
    float4 sh[9];
    uint brdfLutIndex;
    float maxReflectionLod;
    uint shadowMapIndex;
    float _pad;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> globalData;

// --- Set 0: Bindless リソース ---
[[vk::binding(1, 0)]]
Texture2D bindlessTextures[];

[[vk::binding(2, 0)]]
SamplerState defaultSampler;

// --- Set 0: Object Data (SSBO) ---
struct ObjectData {
    float4x4 model_matrix;
    uint data[16];
};

struct PbrMaterialData {
    float4 base_color;
    float metallic;
    float roughness;
    float normal_scale;
    uint albedo_texture_id;
    uint normal_texture_id;
    uint mrm_texture_id;
    uint emissive_texture_id;
};

PbrMaterialData unpackPbrMaterial(ObjectData objData) {
    PbrMaterialData mat;
    mat.base_color = asfloat(uint4(objData.data[0], objData.data[1], objData.data[2], objData.data[3]));
    mat.metallic = asfloat(objData.data[4]);
    mat.roughness = asfloat(objData.data[5]);
    mat.normal_scale = asfloat(objData.data[6]);
    mat.albedo_texture_id = objData.data[7];
    mat.normal_texture_id = objData.data[8];
    mat.mrm_texture_id = objData.data[9];
    mat.emissive_texture_id = objData.data[10];
    return mat;
}

[[vk::binding(3, 0)]]
StructuredBuffer<ObjectData> objectBuffer;

// --- Set 0: IBL リソース ---
[[vk::binding(4, 0)]]
SamplerCube envCubemap;

// ==========================================
// 3. PBR & IBL Helper Functions
// ==========================================
static const float PI = 3.14159265359;

float3 evaluateSphericalHarmonics(float3 N) {
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

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float3 tonemapACESFilmic(float3 x) {
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// フラグメントシェーダー内で法線マップからワールド空間法線を計算する関数
float3 getNormalFromMap(
    Texture2D normalMap,
    SamplerState texSampler,
    float2 uv,
    float3 worldPos,
    float3 worldNormal,
    float normalScale
) {
    // 1. 法線マップのサンプリング [0, 1] -> [-1, 1]
    float3 tangentNormal = normalMap.Sample(texSampler, uv).xyz * 2.0 - 1.0;
    tangentNormal = normalize(float3(tangentNormal.xy * normalScale, tangentNormal.z));

    // 2. 画面微小微分から UV と 座標の傾きを計算
    float3 dPos1 = ddx(worldPos);
    float3 dPos2 = ddy(worldPos);
    float2 dUV1  = ddx(uv);
    float2 dUV2  = ddy(uv);
    // 3. 接線 (Tangent) と 従接線 (Bitangent) を導出
    float3 N = normalize(worldNormal);
    float3 T = normalize(dPos1 * dUV2.y - dPos2 * dUV1.y);
    float3 B = -normalize(cross(N, T)); // 座標系に合わせて符号調整
    float3x3 TBN = float3x3(T, B, N);
    // 4. 接線空間からワールド空間へ変換
    return normalize(mul(tangentNormal, TBN));
}

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
    float3 diffuseIBL = irradiance * albedo * kD_ambient;

    // (B) Specular Radiance (Prefiltered Cubemap + BRDF LUT)
    float3 R = reflect(-V, N);
    float lod = roughness * globalData.maxReflectionLod;
    float3 prefilteredColor = envCubemap.SampleLevel(R, lod).rgb;

    uint brdfLutID = NonUniformResourceIndex(globalData.brdfLutIndex);
    float2 brdf = bindlessTextures[brdfLutID].Sample(defaultSampler, float2(NdotV, roughness)).rg;
    float3 specularIBL = prefilteredColor * (F0 * brdf.x + brdf.y);

    float3 ambient = diffuseIBL + specularIBL;

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
    color = tonemapACESFilmic(color);
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, matData.base_color.a);
}
