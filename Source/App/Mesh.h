#ifndef MESH_H
#define MESH_H

#include "Core.h"
#include "MathUtil.h"
#include "Material.h"
#include "FrameResource.h"

namespace Loxodonta
{

struct Submesh
{
	Material* pMaterial = nullptr;
	uint IndexCount = 0;
	uint StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	std::string Name;
};

struct Mesh
{
	// System memory copies  
	Microsoft::WRL::ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	// Resources on the GPU
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	// Intermediary resources
	Microsoft::WRL::ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	// Data about the buffers.
	uint VertexByteStride = 0;
	uint VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;
	uint IndexBufferByteSize = 0;

	// A MeshGeometry can store multiple geometries in one vertex/index buffer
	std::unordered_map<std::string, Submesh> DrawArgs;

	std::string Name;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;

		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	// We can free this memory after we finish upload to the GPU.
	void DisposeUploaders()
	{
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}

protected:
	void Subdivide(std::vector<Vertex>& vertices, std::vector<u16>& indices);
	Vertex MidPoint(const Vertex& v0, const Vertex& v1);

};

void Mesh::Subdivide(std::vector<Vertex>& vertices, std::vector<u16>& indices)
{
	// Copy of the input mesh
	std::vector<Vertex> inputVertices = vertices;
	std::vector<u16> inputIndices = indices;
	// clear vertices and indices
	vertices.resize(0);
	indices.resize(0);

	u32 numTris = (u32) inputIndices.size()/3;
	for(u32 i = 0; i < numTris; i++)
	{
		Vertex v0 = inputVertices[inputIndices[i*3+0]];
		Vertex v1 = inputVertices[inputIndices[i*3+1]];
		Vertex v2 = inputVertices[inputIndices[i*3+2]];
		// midpoints
		Vertex m0 = MidPoint(v0, v1);
		Vertex m1 = MidPoint(v1, v2);
		Vertex m2 = MidPoint(v0, v2);
		// Add Geometry back in
		vertices.push_back(v0);
		vertices.push_back(v1);
		vertices.push_back(v2);
		vertices.push_back(m0);
		vertices.push_back(m1);
		vertices.push_back(m2);
		// indices
		indices.push_back(i*6+0);
		indices.push_back(i*6+3);
		indices.push_back(i*6+5);

		indices.push_back(i*6+3);
		indices.push_back(i*6+4);
		indices.push_back(i*6+5);

		indices.push_back(i*6+5);
		indices.push_back(i*6+4);
		indices.push_back(i*6+2);

		indices.push_back(i*6+3);
		indices.push_back(i*6+1);
		indices.push_back(i*6+4);
	}

}

Vertex Mesh::MidPoint(const Vertex& v0, const Vertex& v1)
{
	vect p0 = Vector::LoadFloat3(&v0.Pos);
	vect p1 = Vector::LoadFloat3(&v1.Pos);
	vect n0 = Vector::LoadFloat3(&v0.Normal);
	vect n1 = Vector::LoadFloat3(&v1.Normal);
	vect tan0 = Vector::LoadFloat3(&v0.Tangent);
	vect tan1 = Vector::LoadFloat3(&v1.Tangent);
	vect bitan0 = Vector::LoadFloat3(&v0.Bitangent);
	vect bitan1 = Vector::LoadFloat3(&v1.Bitangent);
	vect texC0 = Vector::LoadFloat2(&v0.TexCoord);
	vect texC1 = Vector::LoadFloat2(&v1.TexCoord);

	vect midPos = 0.5f*(p0 + p1);
	vect midNorm = Vector::Normalize3(0.5f*(n0 + n1));
	vect midTan = Vector::Normalize3(0.5f*(tan0 + tan1));
	vect midBitan = Vector::Normalize3(0.5f*(bitan0 + bitan1));
	vect midTexC = 0.5f*(texC0 + texC1);

	Vertex v;
	Vector::StoreFloat3(&v.Pos, midPos);
	Vector::StoreFloat3(&v.Normal, midNorm);
	Vector::StoreFloat3(&v.Tangent, midTan);
	Vector::StoreFloat3(&v.Bitangent, midBitan);
	Vector::StoreFloat2(&v.TexCoord, midTexC);
	return v;
}


/*
struct QuadMesh : public Mesh
{
	float3 Min;
	float3 Max;

	QuadMesh() {}

	QuadMesh(float3 min, float3 max)
		: Min(min), Max(max) { }

	int Init(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
	{
		Vertex vertices[4];
		u16 indices[6];

		float width = Max.x - Min.x;
		float3 minCorner = Min;
		minCorner.x = Max.x;
		float3 maxCorner = Max;
		maxCorner.x = Min.x;

		vect minVect = Vector::LoadFloat3(&Min);
		vect maxVect = Vector::LoadFloat3(&Max);
		vect minCornVect = Vector::LoadFloat3(&minCorner);
		vect normalVector = Vector::CrossProduct3(maxVect - minVect, minCornVect - minVect); // This is LH-ed
		normalVector = Vector::Normalize3(normalVector);

		float3 normal;
		Vector::StoreFloat3(&normal, normalVector);
		// Set Vertices
		vertices[0] = Vertex{
			Min,
			normal,
			{ 0.0f, 1.0f }
		};
		vertices[1] = Vertex{
			maxCorner,
			normal,
			{ 0.0f, 0.0f }
		};
		vertices[2] = Vertex{
			Max,
			normal,
			{ 1.0f, 0.0f }
		};
		vertices[3] = Vertex{
			minCorner,
			normal,
			{ 1.0f, 1.0f }
		};
		// Set Indices
		indices[0] = 0;
		indices[1] = 1;
		indices[2] = 2;
		indices[3] = 0;
		indices[4] = 2;
		indices[5] = 3;

		VertexByteStride = sizeof(Vertex);
		VertexBufferByteSize = (uint) 4 * sizeof(Vertex);
		IndexFormat = DXGI_FORMAT_R16_UINT;
		IndexBufferByteSize = (uint) 6 * sizeof(u16);

		DrawArgs["quad"] = Submesh();
		DrawArgs["quad"].Name = "quad";
		DrawArgs["quad"].IndexCount = 6;
		DrawArgs["quad"].StartIndexLocation = 0;
		DrawArgs["quad"].BaseVertexLocation = 0;

		ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &VertexBufferCPU));
		CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices, VertexBufferByteSize);

		ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &IndexBufferCPU));
		CopyMemory(IndexBufferCPU->GetBufferPointer(), indices, IndexBufferByteSize);

		VertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), vertices, VertexBufferByteSize, VertexBufferUploader);

		IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), indices, IndexBufferByteSize, IndexBufferUploader);

		return 0;
	}
};

// Get from geo gen in Frank Luna
struct GridMesh : public Mesh
{
	float3 Min;
	float3 Max;
	uint M;
	uint N;

	GridMesh() {}

	GridMesh(float3 min, float3 max, uint m, uint n)
		: Min(min), Max(max), M(m), N(n) {	}

	int Init(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
	{
		uint vertexCount = M*N;
		uint faceCount = (M-1)*(N-1) * 2;

		//
		// Create Vertices
		//
		float3 bisect = { Max.x - Min.x, Max.y - Min.y, Max.z - Min.z };
		float3 delta = {bisect.x / (N-1), bisect.y / (M-1), bisect.z / (M-1)};
		float2 deltaUV = { 1.0f / (N - 1), 1.0f / (M - 1) };
		// Get vertices' normal
		float3 minCorner = Min;
		minCorner.x = Max.x;
		float3 maxCorner = Max;
		maxCorner.x = Min.x;
		vect minVect = Vector::LoadFloat3(&Min);
		vect maxVect = Vector::LoadFloat3(&Max);
		vect minCornVect = Vector::LoadFloat3(&minCorner);
		vect normalVector = Vector::CrossProduct3(maxVect - minVect, minCornVect - minVect); // This is LH-ed
		normalVector = Vector::Normalize3(normalVector);
		float3 normal;
		Vector::StoreFloat3(&normal, normalVector);

		std::vector<Vertex> vertices(vertexCount);
		for(uint i = 0; i < M; i++)
		{
			for(uint j = 0; j < N; j++)
			{
				float x = maxCorner.x + j*delta.x;
				vertices[i*N + j].Pos = {
					maxCorner.x + j*delta.x,
					maxCorner.y - i*delta.y,
					maxCorner.z - i*delta.z };
				vertices[i*N + j].Normal = normal;
				// Set the tex coords
				vertices[i*N + j].TexCoord = { j * deltaUV.x, i * deltaUV.y };
			}
		}

		//
		// Create Indices
		//
		std::vector<u32> indices(faceCount * 3); // 3 indices per face
		uint k = 0;

		for(uint i = 0; i < M-1; i++)
		{
			for(uint j = 0; j < N-1; j++)
			{
				indices[k++] = i*N + j;
				indices[k++] = i*N + j+1;
				indices[k++] = (i+1)*N + j;
				indices[k++] = (i+1)*N + j;
				indices[k++] = i*N + j+1;
				indices[k++] = (i+1)*N + j+1;
				// on to the next quad
			}
		}

		//
		// Set hardware parameters
		//
		DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
		uint indexTypeSize = sizeof(u32);
		std::vector<u16> indices16 = std::vector<u16>();
		if (vertexCount <= 0xFFFF)
		{ // Convert to 16 bit indices
			indices16.resize(faceCount * 3);
			for (uint i = 0; i < indices.size(); i++)
			{
				indices16[i] = static_cast<u16>(indices[i]);
			}
			indexFormat = DXGI_FORMAT_R16_UINT;
			indexTypeSize = sizeof(u16);
		}

		VertexByteStride = sizeof(Vertex);
		VertexBufferByteSize = (uint) vertexCount * sizeof(Vertex);
		IndexFormat = indexFormat;
		IndexBufferByteSize = (uint) faceCount * 3 * indexTypeSize;

		DrawArgs["grid"] = Submesh();
		DrawArgs["grid"].Name = "grid";
		DrawArgs["grid"].IndexCount = faceCount * 3;
		DrawArgs["grid"].StartIndexLocation = 0;
		DrawArgs["grid"].BaseVertexLocation = 0;

		ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &VertexBufferCPU));
		CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);
		VertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), vertices.data(), VertexBufferByteSize, VertexBufferUploader);

		if(vertexCount <= 0xFFFF)
		{
			ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &IndexBufferCPU));
			CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);
			IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
				commandList.Get(), indices16.data(), IndexBufferByteSize, IndexBufferUploader);
		}
		else
		{
			ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &IndexBufferCPU));
			CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);
			IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
				commandList.Get(), indices.data(), IndexBufferByteSize, IndexBufferUploader);
		}

		return 0;
	}
};

struct BoxMesh : public Mesh
{
	float Width;
	float Height;
	float Depth;
	uint NumSubdivisions;

	BoxMesh() {}

	BoxMesh(float width, float height, float depth, uint numSubdivisions)
		: Width(width), Height(height), Depth(depth), NumSubdivisions(numSubdivisions) { }

	int Init(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
	{
		NumSubdivisions = min(NumSubdivisions, 6);

		std::vector<Vertex> vertices(24);
		float w2 = 0.5f*Width;
		float h2 = 0.5f*Height;
		float d2 = 0.5f*Depth;

		// front face
		vertices[ 0] = Vertex({-w2, -h2, -d2}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f});
		vertices[ 1] = Vertex({-w2, +h2, -d2}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f});
		vertices[ 2] = Vertex({+w2, +h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f});
		vertices[ 3] = Vertex({+w2, -h2, -d2}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f});
		// back face
		vertices[ 4] = Vertex({-w2, -h2, +d2}, {0.0f, 0.0f, +1.0f}, {1.0f, 1.0f});
		vertices[ 5] = Vertex({+w2, -h2, +d2}, {0.0f, 0.0f, +1.0f}, {0.0f, 1.0f});
		vertices[ 6] = Vertex({+w2, +h2, +d2}, {0.0f, 0.0f, +1.0f}, {0.0f, 0.0f});
		vertices[ 7] = Vertex({-w2, +h2, +d2}, {0.0f, 0.0f, +1.0f}, {1.0f, 0.0f});
		// top face
		vertices[ 8] = Vertex({-w2, +h2, -d2}, {0.0f, +1.0f, 0.0f}, {0.0f, 1.0f});
		vertices[ 9] = Vertex({-w2, +h2, +d2}, {0.0f, +1.0f, 0.0f}, {0.0f, 0.0f});
		vertices[10] = Vertex({+w2, +h2, +d2}, {0.0f, +1.0f, 0.0f}, {1.0f, 0.0f});
		vertices[11] = Vertex({+w2, +h2, -d2}, {0.0f, +1.0f, 0.0f}, {1.0f, 1.0f});
		// bottom face
		vertices[12] = Vertex({-w2, -h2, -d2}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f});
		vertices[13] = Vertex({+w2, -h2, -d2}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f});
		vertices[14] = Vertex({+w2, -h2, +d2}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f});
		vertices[15] = Vertex({-w2, -h2, +d2}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f});
		// left face
		vertices[16] = Vertex({-w2, -h2, +d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f});
		vertices[17] = Vertex({-w2, +h2, +d2}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f});
		vertices[18] = Vertex({-w2, +h2, -d2}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f});
		vertices[19] = Vertex({-w2, -h2, -d2}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});
		// right face
		vertices[20] = Vertex({+w2, -h2, -d2}, {+1.0f, 0.0f, 0.0f}, {0.0f, 1.0f});
		vertices[21] = Vertex({+w2, +h2, -d2}, {+1.0f, 0.0f, 0.0f}, {0.0f, 0.0f});
		vertices[22] = Vertex({+w2, +h2, +d2}, {+1.0f, 0.0f, 0.0f}, {1.0f, 0.0f});
		vertices[23] = Vertex({+w2, -h2, +d2}, {+1.0f, 0.0f, 0.0f}, {1.0f, 1.0f});

		std::vector<u16> indices(36);
		// front face
		indices[0] = 0; indices[1] = 1; indices[2] = 2;
		indices[3] = 0; indices[4] = 2; indices[5] = 3;
		// back face
		indices[6] = 4; indices[7] = 5; indices[8] = 6;
		indices[9] = 4; indices[10] = 6; indices[11] = 7;
		// top face
		indices[12] = 8; indices[13] = 9; indices[14] = 10;
		indices[15] = 8; indices[16] = 10; indices[17] = 11;
		// bottom face
		indices[18] = 12; indices[19] = 13; indices[20] = 14;
		indices[21] = 12; indices[22] = 14; indices[23] = 15;
		// left face
		indices[24] = 16; indices[25] = 17; indices[26] = 18;
		indices[27] = 16; indices[28] = 18; indices[29] = 19;
		// right face
		indices[30] = 20; indices[31] = 21; indices[32] = 22;
		indices[33] = 20; indices[34] = 22; indices[35] = 23;


		for(uint i = 0; i < NumSubdivisions; i++)
			Subdivide(vertices, indices);

		VertexByteStride = sizeof(Vertex);
		VertexBufferByteSize = (uint) vertices.size() *sizeof(Vertex);
		IndexFormat = DXGI_FORMAT_R16_UINT;
		IndexBufferByteSize = (uint) indices.size() *sizeof(u16);

		DrawArgs["box"] = Submesh();
		DrawArgs["box"].Name = "box";
		DrawArgs["box"].IndexCount = indices.size();
		DrawArgs["box"].StartIndexLocation = 0;
		DrawArgs["box"].BaseVertexLocation = 0;

		ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &VertexBufferCPU));
		CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);

		ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &IndexBufferCPU));
		CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

		VertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), vertices.data(), VertexBufferByteSize, VertexBufferUploader);

		IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), indices.data(), IndexBufferByteSize, IndexBufferUploader);

		return 0;
	}
};
*/

struct SphereMesh : public Mesh
{
	float Radius;
	u8 SliceCount;
	u8 StackCount;
	u16 __PAD;

	SphereMesh() {}

	SphereMesh(float radius, u8 sliceCount, u8 stackCount)
		: Radius(radius), SliceCount(sliceCount), StackCount(stackCount) { }

	int Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
	{
		assert(SliceCount <= 250 && StackCount <= 250);

		std::vector<Vertex> vertices(0);
		std::vector<u16> indices(0);

		Vertex northPole = Vertex({0.0f, +Radius, 0.0f},                   // Position
			{0.0f, +1.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, // normal, tan, bitan
			{0.0f, 0.0f});                                                 // UV
		Vertex southPole = Vertex({0.0f, -Radius, 0.0f}, 
			{0.0f, -1.0f, 0.0f}, {+1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, +1.0f}, 
			{0.0f, 1.0f});

		float deltaPhi   = M_PI / StackCount;
		float deltaTheta = M_2PI / SliceCount;

		vertices.push_back(northPole);
		for(u8 i = 1; i <= StackCount - 1; i++)
		{
			float phi = i * deltaPhi;
			for(u8 j = 0; j <= SliceCount; j++)
			{
				float theta = j*deltaTheta;

				Vertex v;
				v.Pos.x = Radius*sinf(phi)*cosf(theta);
				v.Pos.y = Radius*cosf(phi);
				v.Pos.z = Radius*sinf(phi)*sinf(theta);
				vect normal = Vector::Normalize3(Vector::LoadFloat3(&v.Pos));				
				Vector::StoreFloat3(&v.Normal, normal);
				// Partial derivative of P with respect to theta
				v.Tangent.x = -Radius*sinf(phi)*sinf(theta);
				v.Tangent.y = 0.0f;
				v.Tangent.z = Radius*sinf(phi)*cosf(theta);
				// Bitangent = cross(normal, tangent)
				vect tangent = Vector::LoadFloat3(&v.Tangent);
				vect bitangent = Vector::CrossProduct3(normal, tangent);
				Vector::StoreFloat3(&v.Bitangent, bitangent);
				v.TexCoord.x = theta / M_2PI;
				v.TexCoord.y = phi / M_PI;

				vertices.push_back(v);
			}
		}
		vertices.push_back(southPole);

		// indices for top stack
		for(u8 i = 1; i <= SliceCount; i++)
		{
			indices.push_back(0);
			indices.push_back(i+1);
			indices.push_back(i);
		}
		// indices for inner stacks
		uint baseIndex = 1;
		uint ringVertexCount = SliceCount + 1;
		for(u8 i = 0; i < StackCount-2; i++)
		{
			for(u8 j = 0; j < SliceCount; j++)
			{
				indices.push_back(baseIndex + i*ringVertexCount + j);
				indices.push_back(baseIndex + i*ringVertexCount + j+1);
				indices.push_back(baseIndex + (i+1)*ringVertexCount + j);

				indices.push_back(baseIndex + (i+1)*ringVertexCount + j);
				indices.push_back(baseIndex + i*ringVertexCount + j+1);
				indices.push_back(baseIndex + (i+1)*ringVertexCount + j+1);
			}
		}
		// indices for bottom stack
		uint southPoleIndex = (uint)vertices.size()-1;
		baseIndex = southPoleIndex - ringVertexCount;
		for(u8 i = 0; i < SliceCount; i++)
		{
			indices.push_back(southPoleIndex);
			indices.push_back(baseIndex+i);
			indices.push_back(baseIndex+i+1);
		}

		VertexByteStride = sizeof(Vertex);
		VertexBufferByteSize = (uint) vertices.size() *sizeof(Vertex);
		IndexFormat = DXGI_FORMAT_R16_UINT;
		IndexBufferByteSize = (uint) indices.size() *sizeof(u16);

		DrawArgs["sphere"] = Submesh();
		DrawArgs["sphere"].Name = "sphere";
		DrawArgs["sphere"].IndexCount = indices.size();
		DrawArgs["sphere"].StartIndexLocation = 0;
		DrawArgs["sphere"].BaseVertexLocation = 0;

		ThrowIfFailed(D3DCreateBlob(VertexBufferByteSize, &VertexBufferCPU));
		CopyMemory(VertexBufferCPU->GetBufferPointer(), vertices.data(), VertexBufferByteSize);

		ThrowIfFailed(D3DCreateBlob(IndexBufferByteSize, &IndexBufferCPU));
		CopyMemory(IndexBufferCPU->GetBufferPointer(), indices.data(), IndexBufferByteSize);

		VertexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), vertices.data(), VertexBufferByteSize, VertexBufferUploader);

		IndexBufferGPU = d3dUtil::CreateDefaultBuffer(d3dDevice.Get(),
			commandList.Get(), indices.data(), IndexBufferByteSize, IndexBufferUploader);

		return 0;
	}
};

// @todo
struct GeosphereMesh : public Mesh
{
	
};

struct CylinderMesh : public Mesh
{

};

struct CapsuleMesh : public Mesh
{
	
};

// Get from loadmodel
struct PolygonalMesh : public Mesh
{

};



}

#endif //!MESH_H