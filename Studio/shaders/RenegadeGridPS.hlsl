// Renegade editor grid - pixel stage.
//
// Analytic infinite ground grid. For each pixel it reconstructs a world-space
// view ray, intersects the y = 0 plane, and derives line coverage from
// screen-space derivatives so lines stay one pixel wide and anti-aliased at
// any distance or grazing angle. Spacing adapts in powers of ten as the camera
// pulls back, cross-fading between decades so the change is not a visible pop.
//
// Depth: the hit point is projected with the same view-projection matrix the
// scene used and written to SV_Depth. The hardware depth test then occludes
// the grid behind scene geometry, so no depth texture needs binding and the
// reverse-Z convention is inherited rather than reimplemented.

struct VertexOutput
{
	float4 position : SV_Position;
	float2 clipXY   : TEXCOORD0;
};

struct PixelOutput
{
	float4 color : SV_Target;
	float  depth : SV_Depth;
};

cbuffer RenegadeGridCB : register(b0)
{
	float4x4 g_viewProjection;
	float4x4 g_inverseViewProjection;
	float4   g_cameraPosition;   // xyz = world position, w = grid plane height
	float4   g_minorColor;       // rgb + a = peak opacity
	float4   g_majorColor;
	float4   g_axisColorX;
	float4   g_axisColorZ;
	float4   g_params;           // x = fade start, y = fade end,
	                             // z = base spacing, w = master opacity
};

// World position where the ray through a clip-space point meets the grid
// plane. Returns false when the ray is parallel to, or points away from, it.
//
// The plane sits a hair above y = 0 rather than on it. The generated Proving
// Ground deck's top surface is at exactly y = 0, so a grid at y = 0 is
// coplanar with it and loses the depth test - DSSTYPE_DEPTHREAD compares with
// GREATER under reverse Z, and equal depths are not greater. Wicked's own
// helper carries the same offset for the same reason:
//   const float h = 0.01f; // avoid z-fight with zero plane
bool IntersectGroundPlane(float2 clipXY, out float3 worldPosition)
{
	worldPosition = float3(0.0, 0.0, 0.0);

	// Unproject two points at different depths to build the ray. Wicked uses a
	// reverse-Z projection, so z = 1 is the near plane and z = 0 is the far
	// plane; taking both and subtracting avoids depending on which is which.
	float4 nearPoint = mul(g_inverseViewProjection, float4(clipXY, 1.0, 1.0));
	float4 farPoint  = mul(g_inverseViewProjection, float4(clipXY, 0.0, 1.0));

	if (abs(nearPoint.w) < 1e-8 || abs(farPoint.w) < 1e-8)
	{
		return false;
	}

	const float3 origin    = nearPoint.xyz / nearPoint.w;
	const float3 target    = farPoint.xyz / farPoint.w;
	const float3 direction = target - origin;

	// Parallel to the ground plane.
	if (abs(direction.y) < 1e-8)
	{
		return false;
	}

	const float planeHeight = g_cameraPosition.w;
	const float t = (planeHeight - origin.y) / direction.y;

	// Behind the camera, or beyond the far plane.
	if (t < 0.0 || t > 1.0)
	{
		return false;
	}

	worldPosition = origin + direction * t;
	return true;
}

// Anti-aliased line coverage for a grid of the given spacing. Uses the
// screen-space derivative of the world position so a line occupies roughly one
// pixel however far away it is - the reason this exists rather than a line
// list.
float GridCoverage(float2 groundXZ, float2 derivative, float spacing)
{
	const float2 scaled     = groundXZ / spacing;
	const float2 width      = derivative / spacing;
	const float2 distanceToLine = abs(frac(scaled - 0.5) - 0.5);
	const float2 coverage   = distanceToLine / max(width, 1e-8);
	return 1.0 - saturate(min(coverage.x, coverage.y));
}

// Coverage of a single axis line lying on the given world coordinate.
float AxisCoverage(float coordinate, float derivative)
{
	return 1.0 - saturate(abs(coordinate) / max(derivative, 1e-8));
}

// ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT matches Wicked's Example_ImGui. Without
// it, Wicked's DX12 backend fails to build the indirect draw command
// signatures it derives from an embedded root signature, logging
// CreateCommandSignature E_INVALIDARG errors at startup. Those indirect paths
// are never used by this shader, but the error spam would hide real problems.
[RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT), CBV(b0)")]
PixelOutput main(VertexOutput input)
{
	PixelOutput output;
	output.color = float4(0.0, 0.0, 0.0, 0.0);
	output.depth = 0.0;

	float3 groundPosition;
	if (!IntersectGroundPlane(input.clipXY, groundPosition))
	{
		discard;
		return output;
	}

	// Distance fade. Beyond fadeEnd there is nothing to draw, which also keeps
	// the horizon from turning into an aliased smear.
	const float distanceToCamera = length(groundPosition - g_cameraPosition.xyz);
	const float fadeStart = g_params.x;
	const float fadeEnd   = max(g_params.y, fadeStart + 1e-3);
	const float fade =
		1.0 - saturate((distanceToCamera - fadeStart) / (fadeEnd - fadeStart));
	if (fade <= 0.0)
	{
		discard;
		return output;
	}

	// Rate of change of world position per pixel, used for both the adaptive
	// spacing decision and the line width.
	const float2 groundXZ = groundPosition.xz;
	const float2 derivative =
		max(abs(ddx(groundXZ)), abs(ddy(groundXZ))) + 1e-8;
	const float texelWorldSize = max(derivative.x, derivative.y);

	// Choose a decade of spacing so minor lines stay legible, and cross-fade
	// into the next decade rather than popping.
	const float baseSpacing = max(g_params.z, 1e-3);
	const float decade = log10(max(texelWorldSize / baseSpacing, 1e-6)) + 1.0;
	const float decadeFloor = floor(decade);
	const float decadeBlend = saturate(decade - decadeFloor);

	const float spacingNear = baseSpacing * pow(10.0, max(decadeFloor, 0.0));
	const float spacingFar  = spacingNear * 10.0;

	const float coverageNear = GridCoverage(groundXZ, derivative, spacingNear);
	const float coverageFar  = GridCoverage(groundXZ, derivative, spacingFar);

	// Minor lines fade out as their decade becomes too dense to read.
	const float minorCoverage = coverageNear * (1.0 - decadeBlend);
	// Major lines are every tenth minor line of the coarser decade.
	const float majorCoverage = coverageFar;

	float4 color = g_minorColor;
	color.a *= minorCoverage;

	float4 major = g_majorColor;
	major.a *= majorCoverage;

	// Composite major over minor.
	color.rgb = lerp(color.rgb, major.rgb, major.a);
	color.a = saturate(color.a + major.a);

	// Axis lines last so they always read on top.
	const float axisZ = AxisCoverage(groundXZ.x, derivative.x); // x = 0 runs along Z
	const float axisX = AxisCoverage(groundXZ.y, derivative.y); // z = 0 runs along X

	float4 axis = g_axisColorZ;
	axis.a *= axisZ;
	color.rgb = lerp(color.rgb, axis.rgb, axis.a);
	color.a = saturate(color.a + axis.a);

	axis = g_axisColorX;
	axis.a *= axisX;
	color.rgb = lerp(color.rgb, axis.rgb, axis.a);
	color.a = saturate(color.a + axis.a);

	color.a *= fade * g_params.w;
	if (color.a <= 0.001)
	{
		discard;
		return output;
	}

	// Project the hit point with the scene's own matrix so the depth written
	// here matches the depth buffer's convention exactly.
	const float4 clipPosition = mul(g_viewProjection, float4(groundPosition, 1.0));
	if (clipPosition.w <= 0.0)
	{
		discard;
		return output;
	}

	output.color = float4(color.rgb * color.a, color.a); // premultiplied
	output.depth = saturate(clipPosition.z / clipPosition.w);
	return output;
}
