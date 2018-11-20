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
    float3 NormalW    : NORMAL;
	float3 TangentW   : TANGENT;
	float3 BitangentW : BINORMAL;
	float2 TexCoord   : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout = (VertexOut)0.0f;
	
	// Transform to world space
	float4 posW = mul(float4(vin.PosL, 1.0f), g_World);
	vout.PosW = posW.xyz;

	// Assumes nonuniform scaling; otherwise use inverse-transpose
	vout.NormalW = mul(vin.NormalL, (float3x3)g_World);

	// Transform tangents and bitangents to world space
	vout.TangentW = mul(vin.TangentU, (float3x3)g_World);
	vout.BitangentW = mul(vin.BitangentU, (float3x3)g_World);

	// Transform to homogeneous clip space.
    vout.PosH = mul(posW, g_ViewProj);
    
	// Output vertex attributes for interpolation across triangle.
    float4 texCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), g_TexTransform);
    vout.TexCoord = mul(texCoord, g_MatTransform).xy;

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    // Interpolating normal can unnormalize it, so renormalize it.
    pin.NormalW = normalize(pin.NormalW);
	
    // Vector from point being lit to eye. 
    float3 V = normalize(g_CameraPosW - pin.PosW);

#if DISPLACEMENT_TEXTURE != 0
/*
	float height = g_TextureArray[5].Sample(g_SamAnisotropicWrap, pin.TexCoord).r;

	float3x3 TBN = float3x3(pin.TangentW, pin.BitangentW, pin.NormalW);

	float3 VinTan = mul(V, TBN);
	float2 P = (VinTan.xy ) * (height * g_HeightScale);
	float2 TexCoord = pin.TexCoord - P;

	clip(TexCoord.x);
	clip(TexCoord.y);
	clip(TexCoord.x > 1.0f ? -1 : 1);
	clip(TexCoord.y > 1.0f ? -1 : 1);
*/
	float2 TexCoord = pin.TexCoord;
#else
	float2 TexCoord = pin.TexCoord;
#endif


	// Fetch the material data
	// Calculate material properties
	// implicit max material properties for all material properties
#if DIFFUSE_TEXTURE != 0
    float3 diffuseAlbedo = g_TextureArray[0].Sample(g_SamAnisotropicWrap, TexCoord).rgb;
#else
	float3 diffuseAlbedo = g_DiffuseAlbedo;
#endif

#if METALLIC_TEXTURE != 0
	float metallic = g_TextureArray[2].Sample(g_SamAnisotropicWrap, TexCoord).r;
#else
	float metallic = g_Metallic;
#endif

#if SPECULAR_TEXTURE != 0
	float3 F0 = g_TextureArray[1].Sample(g_SamAnisotropicWrap, TexCoord).rgb;
#else
	float3 F0 = g_FresnelR0;
	F0 = lerp(F0, diffuseAlbedo, metallic);
#endif

#if ROUGHNESS_TEXTURE != 0
	float roughness = g_TextureArray[3].Sample(g_SamAnisotropicWrap, TexCoord).r;
#else
	float roughness = g_Roughness;
#endif

#if NORMAL_TEXTURE != 0
	float3 N = g_TextureArray[4].Sample(g_SamAnisotropicWrap, TexCoord).rgb;
	N = NormalSampleToWorldSpace(N, pin.NormalW, pin.TangentW, pin.BitangentW);
#else
	float3 N = pin.NormalW;
#endif

#if ALPHA_TEST != 0
	float fragOpacity = g_TextureArray[11].Sample(g_SamPointWrap, TexCoord).r;
	clip(fragOpacity - 0.1f);
#else
	float fragOpacity = g_Opacity;
#endif


    // Light terms.

    Material mat = { 
		diffuseAlbedo,
		metallic,
		F0,
		roughness,
		g_Transmission,
		fragOpacity,
		g_Emissive,
		g_Sheen,
		g_ClearCoatThickness,
		g_ClearCoatRoughness,
		g_Anisotropy,
		g_AnisotropyRotation};

    float3 shadowFactor = 1.0f;
    float3 directLight = ComputeLighting(g_Lights, mat, pin.PosW,
        N, V, shadowFactor);

	// ambient lighting solved wih the IBL
	/*
	float3 kS = FresnelSchlick(N, V, F0);
	float3 kD = 1.0f - kS;
	kD *= (1.0f - metallic);
	float3 irradiance = g_SkyArray[1].Sample(g_SamLinearWrap, WorldToSkyUV(N)).rgb;
	float3 diffuse = irradiance * diffuseAlbedo;
	float3 ambient = (kD * diffuse);

    float3 litColor = ambient + directLight;	
	*/
	float3 litColor = g_AmbientLight * diffuseAlbedo + directLight;

	// HDR tonemapping
	litColor = litColor / (litColor + float3(1.0f, 1.0f, 1.0f));
	// gamma correction
	litColor = pow(litColor,(1.0f/2.2f));
	
    // Common convention to take alpha from diffuse material.
    //litColor.a = diffuseAlbedo.a;

    return float4(litColor.rgb, fragOpacity) ;
}