// Defaults for number of lights.
#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 4
#endif

#ifndef NUM_POINT_LIGHTS
	#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
	#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

Texture2D g_SkyArray[2] : register(t0);

Texture2D g_TextureArray[12] : register(t2);

SamplerState g_SamPointWrap        : register(s0);
SamplerState g_SamPointClamp       : register(s1);
SamplerState g_SamLinearWrap       : register(s2);
SamplerState g_SamLinearClamp      : register(s3);
SamplerState g_SamAnisotropicWrap  : register(s4);
SamplerState g_SamAnisotropicClamp : register(s5);

// Constant data that varies per object
cbuffer cbPerObject : register(b0)
{
	float4x4 g_World;
	float4x4 g_TexTransform;
};

// Constant data that varies per frame
cbuffer cbPass : register(b1)
{
    float4x4 g_View;
    float4x4 g_InvView;
    float4x4 g_Proj;
    float4x4 g_InvProj;
    float4x4 g_ViewProj;
    float4x4 g_InvViewProj;
    float3 g_CameraPosW;
    float __g_pass_PAD000;
    float2 g_RenderTargetSize;
    float2 g_InvRenderTargetSize;
    float g_NearZ;
    float g_FarZ;
    float g_TotalTime;
    float g_DeltaTime;
	float4 g_AmbientLight;
	
	// Allow application to change fog parameters once per frame.
	// For example, we may only use fog for certain times of day.
	float4 g_FogColor;
	float g_FogStart;
	float g_FogRange;
	float2 __g_pass_PAD001;

	Light g_Lights[MAX_LIGHTS];
};

// Constant data that varies per material
cbuffer cbMaterial : register(b2)
{
	float4x4 g_MatTransform;
	float3 g_DiffuseAlbedo;
	float  g_Metallic;
	float3 g_FresnelR0;
	float  g_Roughness;
	float3 g_Transmission;
	float  g_HeightScale;
	float3 g_Emissive;
	float  g_Opacity;
	float  g_Sheen;
	float  g_ClearCoatThickness;
	float  g_ClearCoatRoughness;
	float  g_Anisotropy;
	float  g_AnisotropyRotation;
	float3 __g_mat_pass_PAD000;
}
