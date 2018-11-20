#ifndef TEXTURE_H
#define TEXTURE_H

#include "../3rdParty/DirectXTK12/d3dx12.h"
#include "../3rdParty/DirectXTK12/ResourceUploadBatch.h"
#include "../3rdParty/DirectXTK12/WICTextureLoader.h"
#include "../3rdParty/DirectXTK12/DDSTextureLoader.h"

#include "Core.h"
#include "FrameResource.h"

namespace Loxodonta
{

struct Texture
{
	// Unique material name for lookup
	std::string Name;

	// Index into shader_resource_view heap
	int SRVHeapIndex = -1;

	// Pointer to the resource
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	ResourceUploadBatch ResourceUpload;

	Texture(Microsoft::WRL::ComPtr<ID3D12Device> Device = nullptr)
		: ResourceUpload(ResourceUploadBatch(Device.Get()))
	{
	}
	
	virtual int Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue) = 0;
};

struct ImageTexture : public Texture
{
	std::wstring Filename;

	virtual int Initialize(Microsoft::WRL::ComPtr<ID3D12Device>& d3dDevice,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue>& commandQueue)
	{
		ResourceUpload = ResourceUploadBatch(d3dDevice.Get());
		ResourceUpload.Begin();
		if(Filename.substr(Filename.length() - 4) == L".dds")
		{ // dds format
			ThrowIfFailed(DirectX::CreateDDSTextureFromFile(d3dDevice.Get(), ResourceUpload,
				Filename.c_str(), Resource.ReleaseAndGetAddressOf()));
		}
		else
		{ // otherwise
			ThrowIfFailed(DirectX::CreateWICTextureFromFile(d3dDevice.Get(), ResourceUpload,
				Filename.c_str(), Resource.ReleaseAndGetAddressOf()));
		}
		// Upload the resources to the GPU
		auto RUTexFinished = ResourceUpload.End(commandQueue.Get());
		// Wait for the upload thread to terminate
		RUTexFinished.wait();

		return 0;
	}
};

}
#endif //!TEXTURE_H