//------------------------------------------------//
// Copyright (c) 2026 Nebula-Ray42.               //
// SPDX-License-Identifier: BSD-2-Clause-Patent   //
//------------------------------------------------//

import common_types;
import common_bindings;

struct DepthNormalVertexOutput {
    float4 position : SV_Position;
    float3 normalVS : NORMAL;
};



[shader("vertex")]
DepthNormalVertexOutput vertexMain(VertexInput input, uint instance_id : SV_InstanceID) {
    DepthNormalVertexOutput output;

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
float4 fragmentMain(DepthNormalVertexOutput input) : SV_Target {
    // XYZ = View-Space Normal, W = 1.0
    return float4(normalize(input.normalVS), 1.0);
}
