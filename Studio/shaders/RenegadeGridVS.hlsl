// Renegade editor grid - vertex stage.
//
// Deliberately standalone: no globals.hlsli, no ShaderInterop, no input
// layout. It declares its own root signature and constant buffer so Renegade
// Studio can own a shader without deploying Wicked's shader source tree next
// to the executable. Example_ImGui in the pinned Wicked tree uses the same
// pattern.
//
// Emits one full-screen triangle from SV_VertexID. No vertex buffer is bound;
// the draw call is Draw(3, 0, cmd).

struct VertexOutput
{
	float4 position : SV_Position;
	float2 clipXY   : TEXCOORD0;
};

[RootSignature("RootFlags(0), CBV(b0)")]
VertexOutput main(uint vertexID : SV_VertexID)
{
	// Oversized triangle covering the viewport:
	//   0 -> (-1, -1), 1 -> (3, -1), 2 -> (-1, 3) in clip space.
	const float2 clip = float2(
		(vertexID == 1) ? 3.0 : -1.0,
		(vertexID == 2) ? 3.0 : -1.0);

	VertexOutput output;
	output.position = float4(clip, 0.0, 1.0);
	output.clipXY = clip;
	return output;
}
