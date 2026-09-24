import main;

[shader("vertex")]
VertexOutput mainVert(VertexInput input, uint instance_id : SV_InstanceID)
{
    VertexOutput output;

    ObjectData obj = objectBuffer[instance_id];
    
    // lightViewProj is used instead of viewProj
    output.position = mul(globalData.lightViewProj, mul(obj.model_matrix, float4(input.position, 1.0)));
    
    // We don't really care about other fields for depth pre-pass, but slang requires all outputs if bound
    output.worldPos = mul(obj.model_matrix, float4(input.position, 1.0)).xyz;
    output.color = input.color;
    output.normalDir = normalize(mul((float3x3)obj.model_matrix, input.normalDir));
    output.uv = input.uv;
    output.instanceId = instance_id;

    return output;
}
