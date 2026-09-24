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
    float3 cameraPos;
    float _padding;
    float4 sunDirection; // w = intensity
    float4 sunColor;
    float4 ambientColor;
    float4 sh[9];
    uint brdfLutIndex;
    float maxReflectionLod;
    float2 _pad;
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
    uint albedo_texture_id;
    uint normal_texture_id;
    uint mrm_texture_id;
};

PbrMaterialData unpackPbrMaterial(ObjectData objData) {
    PbrMaterialData mat;
    mat.base_color = asfloat(uint4(objData.data[0], objData.data[1], objData.data[2], objData.data[3]));
    mat.metallic = asfloat(objData.data[4]);
    mat.roughness = asfloat(objData.data[5]);
    mat.albedo_texture_id = objData.data[6];
    mat.normal_texture_id = objData.data[7];
    mat.mrm_texture_id = objData.data[8];
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
    // 3. 最終合成 & トーンマッピング
    // ---------------------------------------------------------
    float3 color = directLight + ambient;

    // HDR トーンマッピング (Reinhard) と ガンマ補正
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0 / 2.2));

    return float4(color, matData.base_color.a);

}
