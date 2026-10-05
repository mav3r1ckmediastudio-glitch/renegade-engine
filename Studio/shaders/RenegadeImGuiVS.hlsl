struct VertexInput
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

struct VertexOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

cbuffer vertexBuffer : register(b0)
{
    float4x4 ProjectionMatrix;
};

// Pinned Wicked DX12 creates counted indirect signatures using root slot 0.
// Reserve one DWORD there, as in its native b999 push-constant convention.
// Both shader stages must embed the same signature; b0 remains the draw CBV.
[RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), RootConstants(num32BitConstants=1, b999), CBV(b0), DescriptorTable(SRV(t0)), DescriptorTable(Sampler(s0))")]
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    output.pos = mul(ProjectionMatrix, float4(input.pos.xy, 0.0f, 1.0f));
    output.uv = input.uv;
    output.col = input.col;
    return output;
}
