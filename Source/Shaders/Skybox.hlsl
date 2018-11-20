#include "Core.hlsl"

struct VertexIn
{
	float3 PosL       : POSITION;
    float3 NormalL    : NORMAL;
	float3 TangentU   : TANGENT;
	float3 BitangentU : BINORMAL;
	float2 TexCoord   : TEXCOORD;
};
struct VertexOut
{
	float4 PosH       : SV_POSITION;
	float3 PosW       : POSITION;
};


VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Use local vertex position as cubemap lookup vector.
	vout.PosW = vin.PosL;
	
	// Transform to world space.
	float4 posW = mul(float4(vin.PosL, 1.0f), g_World);

	// Always center sky about camera.
	posW.xyz += g_CameraPosW;

	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	vout.PosH = mul(posW, g_ViewProj).xyww;
	
	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float3 sampleCoord = normalize(pin.PosW);

	sampleCoord = WorldToSkyUV(sampleCoord);

	float3 skyColor = g_SkyArray[0].Sample(g_SamLinearWrap, sampleCoord);
	// HDR tonemapping
	skyColor = skyColor / (skyColor + float3(1.0f, 1.0f, 1.0f));
	// gamma correction
	skyColor = pow(skyColor,(1.0f/2.2f));
    return float4(skyColor.rgb, 1.0f) ;
}

