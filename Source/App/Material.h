#ifndef MATERIAL_H
#define MATERIAL_H

#include "Core.h"
#include "Texture.h"

namespace Loxodonta
{

struct MaterialProperties
{
	// Used in texture mapping.
	float4x4 MatTransform = MathHelper::Identity4x4();
	// PBR + Blinn-Phong Properties
	float3 Diffuse = {1.0f, 1.0f, 1.0f};
	float Metallic = 0.0f;
	float3 FresnelR0 = {0.04f, 0.04f, 0.04f};
	float Roughness = 1.0f;
	float3 Transmission = {1.0f, 1.0f, 1.0f};
	float HeightScale = 1.0f;
	float3 Emissive = {0.0f, 0.0f, 0.0f};
	float Opacity = 1.0;
	float Sheen = 0.0f;
	float ClearCoatThickness = 0.0f;
	float ClearCoatRoughness = 0.0f;
	float Anisotropy = 0.0f;
	float AnisotropyRotation = 0.0f;
	float3 PAD000;
};

struct Material
{
	Material()
	{
		MatCBIndex = MatCBCount;
		MatCBCount++;
	}

	static int MatCBCount;

	std::string Name;

	// Index into constant buffer corresponding to this material.
	int MatCBIndex = -1;
	
	// Different Possible Textures
	Texture* pDiffuse = nullptr;
	Texture* pSpecular = nullptr;
	Texture* pMetallic = nullptr;
	Texture* pRoughness = nullptr;
	Texture* pNormal = nullptr;
	Texture* pDisplacement = nullptr;
	Texture* pBump = nullptr;
	Texture* pAmbientOcclusion = nullptr;
	Texture* pCavity = nullptr;
	Texture* pSheen = nullptr;
	Texture* pEmissive = nullptr;
	Texture* pOpacity = nullptr;

	// Dirty flag indicating the material has changed and we need to update the constant buffer.
	// Because we have a material constant buffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify a material we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = NUM_FRAME_RESOURCES;

	// Material properties
	//uint PropertiesActive;
	MaterialProperties Properties;
};

int Material::MatCBCount = 0;

}

#endif //!MATERIAL_H