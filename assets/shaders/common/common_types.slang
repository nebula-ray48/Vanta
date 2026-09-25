// types.slang
//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

module common_types;

public struct VertexInput {
    [[vk::location(0)]] public float3 position : POSITION;
    [[vk::location(1)]] public float3 color : COLOR;
    [[vk::location(2)]] public float3 normalDir : NORMAL;
    [[vk::location(3)]] public float2 uv : TEXCOORD;
    [[vk::location(4)]] public uint textureId : TEXINDEX;
};

public struct VertexOutput {
    public float4 position : SV_Position;
    public float3 worldPos : WORLDPOS;
    public float3 color : COLOR;
    public float3 normalDir : NORMAL;
    public float2 uv : TEXCOORD;
    nointerpolation public uint instanceId : INSTANCEID;
};

public struct GlobalUbo {
    public float4x4 viewProj;
    public float4x4 invViewProj;
    public float4x4 lightViewProj;
    public float3 cameraPos;
    public float _padding;
    public float4 sunDirection; // w = intensity
    public float4 sunColor;
    public float4 ambientColor;
    public float4 sh[9];
    public uint brdfLutIndex;
    public float maxReflectionLod;
    public uint shadowMapIndex;
    public uint ssaoMapIndex;
    public float4x4 viewMatrix;
    public float4x4 projMatrix;
    public float4x4 invProjMatrix;
    public float4   samples[64];
    public float2   screenSize;
    public float    radius;
    public float    bias;
    public float    iblIntensity;
    public float    skyboxIntensity;
    public float2   _padding2;
};

public struct ObjectData {
    public float4x4 model_matrix;
    public uint data[16];
};

public struct PbrMaterialData {
    public float4 base_color;
    public float metallic;
    public float roughness;
    public float normal_scale;
    public float occlusion_strength;
    public uint albedo_texture_id;
    public uint normal_texture_id;
    public uint mrm_texture_id;
    public uint emissive_texture_id;
    public uint occlusion_texture_id;
};

// ヘルパー関数: ObjectData から PbrMaterialData に変換する
public PbrMaterialData unpackPbrMaterial(ObjectData objData) {
    PbrMaterialData matData;
    matData.base_color = float4(asfloat(objData.data[0]), asfloat(objData.data[1]), asfloat(objData.data[2]), asfloat(objData.data[3]));
    matData.metallic = asfloat(objData.data[4]);
    matData.roughness = asfloat(objData.data[5]);
    matData.normal_scale = asfloat(objData.data[6]);
    matData.occlusion_strength = asfloat(objData.data[7]);
    matData.albedo_texture_id = objData.data[8];
    matData.normal_texture_id = objData.data[9];
    matData.mrm_texture_id = objData.data[10];
    matData.emissive_texture_id = objData.data[11];
    matData.occlusion_texture_id = objData.data[12];
    return matData;
}
