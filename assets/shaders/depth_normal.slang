//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

struct VertexInput {
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 color : COLOR;
    [[vk::location(2)]] float3 normalDir : NORMAL;
    [[vk::location(3)]] float2 uv : TEXCOORD;
    [[vk::location(4)]] uint textureId : TEXINDEX;
};

struct VertexOutput {
    float4 position : SV_Position;
    float3 normalVS : NORMAL;
};

struct GlobalUbo {
    float4x4 viewProj;
    float4x4 invViewProj;
    float4x4 lightViewProj;
    float3 cameraPos;
    float _padding;
    float4 sunDirection;
    float4 sunColor;
    float4 ambientColor;
    float4 sh[9];
    uint brdfLutIndex;
    float maxReflectionLod;
    uint shadowMapIndex;
    uint ssaoMapIndex;
    
    // 追加: ビュー空間への変換用
    float4x4 viewMatrix;
};

[[vk::binding(0, 0)]]
ConstantBuffer<GlobalUbo> globalData;

struct ObjectData {
    float4x4 model_matrix;
    uint data[16];
};

[[vk::binding(3, 0)]]
StructuredBuffer<ObjectData> objectBuffer;

[shader("vertex")]
VertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID) {
    VertexOutput output;

    float4x4 model = objectBuffer[instance_id].model_matrix;
    float4 worldPos = mul(model, float4(input.position, 1.0));
    
    output.position = mul(globalData.viewProj, worldPos);
    
    // View-Space Normal
    float3 worldNormal = mul((float3x3)model, input.normalDir);
    output.normalVS = mul((float3x3)globalData.viewMatrix, worldNormal);
    output.normalVS = normalize(output.normalVS);

    return output;
}

[shader("fragment")]
float4 fragmentMain(VertexOutput input) : SV_Target {
    // XYZ = View-Space Normal, W = 1.0
    return float4(normalize(input.normalVS), 1.0);
}
