//***************************************************************************************
// LightingUtil.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Contains API for shader lighting.
//***************************************************************************************

#define MAX_LIGHTS 16

struct Light
{
    float3 Strength;
    float SpotPower;    // spot light only
    float3 Direction;   // directional/spot light only
    float __PAD000;
    float3 Position;    // point light only
    float __PAD001;    
};

struct Material
{
	float3 DiffuseAlbedo;
	float Metallic;
	float3 FresnelR0;
	float Roughness;
	float3 Transmission;
	float Opacity;
	float3 Emissive;
	float Sheen;
	float ClearCoatThickness;
	float ClearCoatRoughness;
	float Anisotropy;
	float AnisotropyRotation;
};

float CalcAttenuation(float d)
{
	// Quadratic falloff
	float dSat = max(d, 0.01f);
	return 1 / (dSat*dSat);
}

// Schlick gives an approximation to Fresnel reflectance
float3 FresnelSchlick(float3 H, float3 V, float3 F0)
{
    float cosTheta = saturate(dot(H, V));
	return F0 + (1.0f-F0) * pow(1.0f - cosTheta, 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	roughness = max(roughness, 0.05f);
	float a = roughness*roughness;
	float aSqr = a*a;
	float NdotH = max(dot(N,H), 0.0f);
	float NdotHSqr = NdotH*NdotH;

	float nom = aSqr;
	float denom = (NdotHSqr * (aSqr - 1.0f) + 1.0f);
	denom = 3.14159265359 * denom * denom;

	return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r*r) / 8.0f;
	
	float nom = NdotV;
	float denom = NdotV * (1.0f - k) + k;	

	return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float3 BRDFCookTorrance(Material mat, float3 radiance, float3 N, float3 V, float3 L, float3 H)
{
	float roughness = mat.Roughness;
	float3 F0 = mat.FresnelR0;

	float NDF = DistributionGGX(N, H, roughness);
	float G   = GeometrySmith(N, V, L, roughness);
	float3 F  = FresnelSchlick(H, V, F0); 

	float3 nom = NDF * G * F;
	float denom = 4.0f * max(dot(N,V), 0.0f) * max(dot(N,L) , 0.0f) + 0.001f;
	float3 specular = nom / denom;

	float3 kS = F;
	float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
	kD *= 1.0f - mat.Metallic;

	float NdotL = max(dot(N,L), 0.0f);
	return (kD * mat.DiffuseAlbedo / 3.14159265359 + specular) * radiance * NdotL;
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for directional lights.
//---------------------------------------------------------------------------------------
float3 ComputeDirectionalLight(Light light, Material mat, float3 N, float3 V)
{
	// Calculate per-light radiance
    // The vector from the surface to the light.
    float3 L = -light.Direction;
	// half vector
	float3 H = normalize(V + L);
    float3 radiance = light.Strength;

	return BRDFCookTorrance(mat, radiance, N, V, L, H);	
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for point lights.
//---------------------------------------------------------------------------------------
float3 ComputePointLight(Light light, Material mat, float3 pos, float3 N, float3 V)
{
	// Calculate per-light radiance
    // The vector from the surface to the light.
    float3 L = light.Position - pos;
    float d = length(L);
    // Range test.
    if(d > 100.0f) // Implicit falloff of 100.0f for all lights
        return 0.0f;
    // Normalize the light vector.
    L /= d;
	// half vector
	float3 H = normalize(V + L);
	// Attenuate light by distance.
    float attenuation = CalcAttenuation(d);
    float3 radiance = light.Strength * attenuation;

	return BRDFCookTorrance(mat, radiance, N, V, L, H);	
}

//---------------------------------------------------------------------------------------
// Evaluates the lighting equation for spot lights.
//---------------------------------------------------------------------------------------
float3 ComputeSpotLight(Light light, Material mat, float3 pos, float3 N, float3 V)
{
	// Calculate per-light radiance
    // The vector from the surface to the light.
    float3 L = light.Position - pos;
    float d = length(L);
    // Range test.
    if(d > 100.0f) // Implicit falloff of 100.0f for all lights
        return 0.0f;
    // Normalize the light vector.
    L /= d;
	// half vector
	float3 H = normalize(V + L);
	// Attenuate light by distance.
    float attenuation = CalcAttenuation(d);
	// Attenuate light by angle
	attenuation *= pow(max(dot(-L, light.Direction), 0.0f), light.SpotPower);
    float3 radiance = light.Strength * attenuation;

	return BRDFCookTorrance(mat, radiance, N, V, L, H);	
}


float3 ComputeLighting(Light gLights[MAX_LIGHTS], Material mat,
                       float3 pos, float3 N, float3 V,
                       float3 shadowFactor)
{
    float3 result = 0.0f;

    int i = 0;

#if (NUM_DIR_LIGHTS > 0)
    for(i = 0; i < NUM_DIR_LIGHTS; i++)
    {
        result += shadowFactor * ComputeDirectionalLight(gLights[i], mat, N, V);
    }
#endif

#if (NUM_POINT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS+NUM_POINT_LIGHTS; i++)
    {
        result += ComputePointLight(gLights[i], mat, pos, N, V);
    }
#endif

#if (NUM_SPOT_LIGHTS > 0)
    for(i = NUM_DIR_LIGHTS + NUM_POINT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_POINT_LIGHTS + NUM_SPOT_LIGHTS; i++)
    {
        result += ComputeSpotLight(gLights[i], mat, pos, N, V);
    }
#endif 

    return result;
}

// Transforms a normal map sample to world space
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 N, float3 T, float3 B)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f * normalMapSample - 1.0f;
	
	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

float3 WorldToSkyUV(float3 sampleCoord)
{
	float2 uv = float2(atan2(sampleCoord.z , sampleCoord.x), asin(sampleCoord.y));
	uv = uv * float2(0.1591f, 0.3183f);
	uv = uv + float2(0.5f, 0.5f);
	uv.y = 1.0f - uv.y;
	uv.x = 1.0f - uv.x;
	uv.x = uv.x + 0.25f;
	return float3(uv.x, uv.y, sampleCoord.z);
}