#ifndef FRAME_RESOURCE_H
#define FRAME_RESOURCE_H

#include "../3rdParty/FrankLuna/UploadBuffer.h"

#include "Core.h"
#include "MathUtil.h"
#include "Texture.h"
#include "Material.h"

using namespace Loxodonta;

struct ObjectConstants
{
	float4x4 World = Matrix::Identity4x4();
	float4x4 TexTransform = Matrix::Identity4x4();
};

struct PassConstants
{
	float4x4 View = Matrix::Identity4x4();
	float4x4 InvView = Matrix::Identity4x4();
	float4x4 Proj = Matrix::Identity4x4();
	float4x4 InvProj = Matrix::Identity4x4();
	float4x4 ViewProj = Matrix::Identity4x4();
	float4x4 InvViewProj = Matrix::Identity4x4();
	float3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float cbPerObjectPad1 = 0.0f;
	float2 RenderTargetSize = { 0.0f, 0.0f };
	float2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;

	float4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

	float4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float gFogStart = 5.0f;
	float gFogRange = 150.0f;
	float2 cbPerObjectPad2;

	Light Lights[MaxLights];
};

struct Vertex
{
	float3 Pos;
	float3 Normal;
	float3 Tangent;
	float3 Bitangent;
	float2 TexCoord;

	Vertex() {}

	Vertex(float3 _Pos, float3 _Normal, float3 _Tangent, float3 _Bitangent, float2 _TexCoord)
	{
		Pos = _Pos;
		Normal = _Normal;
		Tangent = _Tangent;
		Bitangent = _Bitangent;
		TexCoord = _TexCoord;
	}

	// Necessary for constructing map of Vertices for loading models
	bool operator==(const Vertex& other) const
	{
		return (
			(Pos.x == other.Pos.x && Pos.y == other.Pos.y && Pos.z == other.Pos.z) && 
			(Normal.x == other.Normal.x && Normal.y == other.Normal.y && Normal.z == other.Normal.z) &&
			(Tangent.x == other.Tangent.x && Tangent.y == other.Tangent.y && Tangent.z == other.Tangent.z) &&
			(Bitangent.x == other.Bitangent.x && Bitangent.y == other.Bitangent.y && Bitangent.z == other.Bitangent.z) &&
			(TexCoord.x == other.TexCoord.x && TexCoord.y == other.TexCoord.y)
		);
	}
};

void hash_combine(size_t &seed, size_t hash)
{
	hash += 0x9e3779b9 + (seed << 6) + (seed >> 2);
	seed ^= hash;
}

namespace std
{
	template<> struct hash<Vertex>
	{
		// Generate the hash for a vertex
		size_t operator()(const Vertex& V) const
		{
			size_t seed = hash<float>{}(V.Pos.x);
			hash_combine(seed, hash<float>{}(V.Pos.y));
			hash_combine(seed, hash<float>{}(V.Pos.z));
			hash_combine(seed, hash<float>{}(V.Normal.x));
			hash_combine(seed, hash<float>{}(V.Normal.y));
			hash_combine(seed, hash<float>{}(V.Normal.z));
			hash_combine(seed, hash<float>{}(V.Tangent.x));
			hash_combine(seed, hash<float>{}(V.Tangent.y));
			hash_combine(seed, hash<float>{}(V.Tangent.z));
			hash_combine(seed, hash<float>{}(V.Bitangent.x));
			hash_combine(seed, hash<float>{}(V.Bitangent.y));
			hash_combine(seed, hash<float>{}(V.Bitangent.z));
			hash_combine(seed, hash<float>{}(V.TexCoord.x));
			hash_combine(seed, hash<float>{}(V.TexCoord.y));
			return seed;
		}
	};
	
}

struct FrameResource
{
public:
	FrameResource(ID3D12Device* Device, UINT passCount, UINT objectCount, UINT materialCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	std::unique_ptr<UploadBuffer<PassConstants>> PassCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialProperties>> MaterialCB = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	UINT64 Fence = 0;
};

FrameResource::FrameResource(ID3D12Device* Device, UINT passCount, UINT objectCount, UINT materialCount)
{
	ThrowIfFailed(Device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf()))
	);

	PassCB = std::make_unique<UploadBuffer<PassConstants>>(Device, passCount, true);
	MaterialCB = std::make_unique<UploadBuffer<MaterialProperties>>(Device, materialCount, true);
	ObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(Device, objectCount, true);
}

FrameResource::~FrameResource() { }

#endif //!FRAME_RESOURCE_H