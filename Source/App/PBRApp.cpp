#pragma once

#include <iostream>
#include <locale>
#include <codecvt>
#include <string>

#include "../3rdParty/FrankLuna/d3dApp.h"
#include "../3rdParty/tinyobjloader/tiny_obj_loader.h"

#include "Core.h"
#include "MathUtil.h"
#include "Camera.h"
#include "FrameResource.h"
#include "Texture.h"
#include "Material.h"
#include "Mesh.h"

  
using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Loxodonta;


const int NUM_FRAME_RESOURCES = 3;

struct RenderItem
{
	RenderItem() = default;

	// Object's local space to World Space
	float4x4 World = Matrix::Identity4x4();
	float4x4 TexTransform = Matrix::Identity4x4();

	int numFramesDirty = NUM_FRAME_RESOURCES;
	uint objCBIndex = -1;

	Material* Mat = nullptr;
	Mesh* Geo = nullptr;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	uint indexCount = 0;
	uint startIndexLocation = 0;
	int baseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	OpaqueAmrn,
	OpaqueAsrnd,
	OpaqueTextureless,
	Transparent,
	AlphaTested,
	SkyBox,
	Count
};

class PBRApp : public D3DApp
{
public:
	PBRApp(HINSTANCE hInstance);
	PBRApp(const PBRApp& rhs) = delete;
	PBRApp& operator=(const PBRApp& rhs) = delete;
	~PBRApp();

	virtual bool Initialize()override;

private:
	virtual void OnResize()override;
	void OnKeyboardInput(const GameTimer& gt);
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseWheel(WPARAM btnState, short delta) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

	void LoadTextures();
	void LoadModels();
	void LoadOBJModel(std::string filepath);
	void AddImageTexture(Texture *&pTexture, std::string basepath, std::string texName);

	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildCamera();
	void BuildShadersAndInputLayout();
	void BuildMaterials();	
	void BuildGeometry();
	void BuildRenderItems();
	void BuildFrameResources();
	void BuildPSOs();

	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);

	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	
private:

	std::vector <std::unique_ptr<FrameResource>> m_FrameResources;
	FrameResource* m_CurrFrameResource = nullptr;
	int m_currFrameResourceIndex = 0;

	uint m_cbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> m_RootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> m_SrvDescriptorHeap = nullptr;

	Camera m_Camera;

	std::unordered_map<std::string, std::unique_ptr<Mesh>> m_Meshes;
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;
	std::unordered_map<std::string, std::vector<std::unique_ptr<Texture>>> m_Textures;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> m_Shaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> m_PSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_InputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> m_AllRenderItems;

	// Render items divided by PSO
	std::vector<RenderItem*> m_RenderItemLayer[(int) RenderLayer::Count];
	
	PassConstants m_MainPassCB;    
	
	POINT m_LastMousePos;
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		PBRApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}



PBRApp::PBRApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

PBRApp::~PBRApp()
{
	if (m_D3dDevice != nullptr)
		FlushCommandQueue();
}

bool PBRApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(m_CommandList->Reset(m_DirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type
	m_cbvSrvDescriptorSize = m_D3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	LoadTextures();
	//LoadModels();

	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildCamera();
	BuildShadersAndInputLayout();
	BuildMaterials();
	BuildGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(m_CommandList->Close());
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void PBRApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	m_Camera.OnResize(m_clientWidth, m_clientHeight);
}

void PBRApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	// Cycle through the circular frame resource array
	m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % NUM_FRAME_RESOURCES;
	m_CurrFrameResource = m_FrameResources[m_currFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current from resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if(m_CurrFrameResource->Fence != 0 && m_Fence->GetCompletedValue() < m_CurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(m_Fence->SetEventOnCompletion(m_CurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void PBRApp::Draw(const GameTimer& gt)
{
	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	auto cmdListAlloc = m_CurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording
	// We can only reset when the associated command lists have finished execution on the GPU
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (m_isWireframe)
	{
		ThrowIfFailed(m_CommandList->Reset(cmdListAlloc.Get(), m_PSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(m_CommandList->Reset(cmdListAlloc.Get(), m_PSOs["opaque"].Get()));
	}

	m_CommandList->RSSetViewports(1, &m_ScreenViewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	float clearColor[] = {0.5f, 0.5f, 0.5f, 1.0f};
	m_CommandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);
	m_CommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	m_CommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* DescriptorHeaps[] = { m_SrvDescriptorHeap.Get() };
	m_CommandList->SetDescriptorHeaps(_countof(DescriptorHeaps), DescriptorHeaps);

	m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

	// Bind per pass constant buffer. We only need to do this once per pass
	auto passCB = m_CurrFrameResource->PassCB->Resource();
	m_CommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	// Opaque Draw pass
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int)RenderLayer::Opaque]);	
	// Opaque ASMRND Draw Pass
	if(!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["opaqueAsrnd"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::OpaqueAsrnd]);
	// Opaque DMRN Draw Pass
	if(!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["opaqueAmrn"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::OpaqueAmrn]);
	// Opaque Textureless Draw Pass
	if(!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["opaqueTextureless"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::OpaqueTextureless]);


	// AlphaTested Draw pass
	if(!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["alphaTested"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::AlphaTested]);

	// Transparent Draw pass
	if (!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["transparent"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::Transparent]);

	if (!m_isWireframe)
		m_CommandList->SetPipelineState(m_PSOs["skybox"].Get());
	DrawRenderItems(m_CommandList.Get(), m_RenderItemLayer[(int) RenderLayer::SkyBox]);

	// Indicate a state transition on the resource usage.
	m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(m_CommandList->Close());

	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_CommandList.Get() };
	m_CommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	// swap the back and front buffers
	ThrowIfFailed(m_SwapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;
	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	// Advance  the fence value to mark commands up to this fence point.
	m_CurrFrameResource->Fence = ++m_currentFence;
	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

	// Add an instruction to the command queue to set a new fence point.
	// Because we are on the GPU timeline, the new fence point won't be
	// set until the GPU finishes processing all the commands prior to this Signal().
	m_CommandQueue->Signal(m_Fence.Get(), m_currentFence);

	ThrowIfFailed(m_D3dDevice.Get()->GetDeviceRemovedReason());

}

void PBRApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	m_LastMousePos.x = x;
	m_LastMousePos.y = y;

	SetCapture(m_hMainWnd);
}

void PBRApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

// @todo
void PBRApp::OnMouseWheel(WPARAM btnState, short delta)
{
}

void PBRApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float deltaYaw = Math::DegreesToRadians(0.25f*static_cast<float>(x - m_LastMousePos.x));
		float deltaPitch = Math::DegreesToRadians(-0.25f*static_cast<float>(y - m_LastMousePos.y));

		m_Camera.ProcessMouseMovement(deltaYaw, deltaPitch);
	}

	m_LastMousePos.x = x;
	m_LastMousePos.y = y;
}

void PBRApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	float deltaSide = 0.0f;
	float deltaForward = 0.0f;
	// Move the camera
	if (GetAsyncKeyState('A') & 0x8000)
		deltaSide = 5.0f * dt;
	if (GetAsyncKeyState('D') & 0x8000)
		deltaSide = -5.0f * dt;
	if (GetAsyncKeyState('W') & 0x8000)
		deltaForward = 5.0f * dt;
	if (GetAsyncKeyState('S') & 0x8000)
		deltaForward = -5.0f * dt;
	m_Camera.ProcessKeyboardInput(deltaSide, deltaForward);
}

void PBRApp::UpdateCamera(const GameTimer& gt)
{
	// Solve for View Matrix
	m_Camera.DeriveViewMatrix();
}

void PBRApp::AnimateMaterials(const GameTimer& gt)
{
	
}


void PBRApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = m_CurrFrameResource->ObjectCB.get();
	for(auto& Item : m_AllRenderItems)
	{
		// loop through all render items
		if(Item->numFramesDirty > 0)
		{
			// if they have a dirty frame, update the object CB
			vect4 World = Matrix::LoadFloat4x4(&Item->World);
			vect4 TexTransform = Matrix::LoadFloat4x4(&Item->TexTransform);

			ObjectConstants objConstants;
			Matrix::StoreFloat4x4(&objConstants.World,        Matrix::Transpose(World));
			Matrix::StoreFloat4x4(&objConstants.TexTransform, Matrix::Transpose(TexTransform));

			currObjectCB->CopyData(Item->objCBIndex, objConstants);

			Item->numFramesDirty--;
		}
	}
}

void PBRApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto CurrMaterialCB = m_CurrFrameResource->MaterialCB.get();
	for (auto& M : m_Materials)
	{
		// Only update the cbuffer data if the constants have changed
		Material* Mat = M.second.get();
		if (Mat->NumFramesDirty > 0)
		{
			CurrMaterialCB->CopyData(Mat->MatCBIndex, Mat->Properties);
			Mat->NumFramesDirty--;
		}
	}
}

void PBRApp::UpdateMainPassCB(const GameTimer& gt)
{
	vect4 View = m_Camera.GetViewMatrix();
	vect4 Proj = m_Camera.GetProjectionMatrix();
	vect4 ViewProj = Matrix::Multiply(View, Proj);
	vect4 InvView = Matrix::Inverse(&Matrix::Determinant(View), View);
	vect4 InvProj = Matrix::Inverse(&Matrix::Determinant(Proj), Proj);
	vect4 InvViewProj = Matrix::Inverse(&Matrix::Determinant(ViewProj), ViewProj);

	Matrix::StoreFloat4x4(&m_MainPassCB.View,        Matrix::Transpose(View));
	Matrix::StoreFloat4x4(&m_MainPassCB.InvView,     Matrix::Transpose(InvView));
	Matrix::StoreFloat4x4(&m_MainPassCB.Proj,        Matrix::Transpose(Proj));
	Matrix::StoreFloat4x4(&m_MainPassCB.InvProj,     Matrix::Transpose(InvProj));
	Matrix::StoreFloat4x4(&m_MainPassCB.ViewProj,    Matrix::Transpose(ViewProj));
	Matrix::StoreFloat4x4(&m_MainPassCB.InvViewProj, Matrix::Transpose(InvViewProj));
	Vector::StoreFloat3(&m_MainPassCB.EyePosW, m_Camera.GetPosition());
	m_MainPassCB.RenderTargetSize = float2((float) m_clientWidth, (float) m_clientHeight);
	m_MainPassCB.InvRenderTargetSize = float2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
	m_MainPassCB.NearZ = m_Camera.GetNearZ();
	m_MainPassCB.FarZ = m_Camera.GetFarZ();
	m_MainPassCB.TotalTime = gt.TotalTime();
	m_MainPassCB.DeltaTime = gt.DeltaTime();

	m_MainPassCB.AmbientLight = { 0.03f, 0.03f, 0.03f, 1.0f };
	// Direction Light
	m_MainPassCB.Lights[0].Direction = { 0.57735f, 0.57735f, 0.57735f };
	m_MainPassCB.Lights[0].Strength = {0.25f, 0.25f, 0.25f };
	m_MainPassCB.Lights[1].Direction = {0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[1].Strength = {0.25f, 0.25f, 0.25f};
	m_MainPassCB.Lights[2].Direction = {-0.57735f, 0.57735f, 0.57735f};
	m_MainPassCB.Lights[2].Strength = {0.25f, 0.25f, 0.25f};
	m_MainPassCB.Lights[3].Direction = {-0.57735f, -0.57735f, 0.57735f};
	m_MainPassCB.Lights[3].Strength = {0.25f, 0.25f, 0.25f};

	// Point Lights
	//m_MainPassCB.Lights[0].Position = { 20.0f,  20.0f, -20.0f };
	//m_MainPassCB.Lights[0].Strength = { 100.0f,  100.0f,  100.0f };
	//m_MainPassCB.Lights[1].Position = {-20.0f,  20.0f, -20.0f};
	//m_MainPassCB.Lights[1].Strength = { 100.0f,  100.0f,  100.0f };
	//m_MainPassCB.Lights[2].Position = { 20.0f, -20.0f, -20.0f};
	//m_MainPassCB.Lights[2].Strength = { 100.0f,  100.0f,  100.0f};
	//m_MainPassCB.Lights[3].Position = {-20.0f, -20.0f, -20.0f};
	//m_MainPassCB.Lights[3].Strength = { 100.0f,  100.0f,  100.0f};
	
	// Copy over pass cb
	auto currPassCB = m_CurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, m_MainPassCB);
}

void PBRApp::BuildGeometry()
{
	// Build all of the spheres for the render task
	auto skyBox = std::make_unique<SphereMesh>();
	skyBox->Name = "sky_box";
	skyBox->Radius = 1.0f;
	skyBox->SliceCount = 64;
	skyBox->StackCount = 32;
	skyBox->Initialize(m_D3dDevice, m_CommandList);
	skyBox->DrawArgs["sphere"].pMaterial = m_Materials[skyBox->Name].get();
	m_Meshes[skyBox->Name] = std::move(skyBox);
	auto sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_rust";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_rock_copper";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_brick_modern";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_concrete_dirty";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_concrete_rough";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_grass_wild";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_metal_bare";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_soil_mud";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);
	sphere = std::make_unique<SphereMesh>();
	sphere->Name = "sphere_stone_wall";
	sphere->Radius = 1.0f;
	sphere->SliceCount = 64;
	sphere->StackCount = 32;
	sphere->Initialize(m_D3dDevice, m_CommandList);
	sphere->DrawArgs["sphere"].pMaterial = m_Materials[sphere->Name].get();
	m_Meshes[sphere->Name] = std::move(sphere);

	for(int i = 0; i < 49; i++)
	{
		auto redSphere = std::make_unique<SphereMesh>();
		redSphere->Name = "sphere_red_" + std::to_string(i);
		redSphere->Radius = 1.0f;
		redSphere->SliceCount = 64;
		redSphere->StackCount = 32;
		redSphere->Initialize(m_D3dDevice, m_CommandList);
		redSphere->DrawArgs["sphere"].pMaterial = m_Materials[redSphere->Name].get();
		m_Meshes[redSphere->Name] = std::move(redSphere);
	}
}


void PBRApp::LoadModels()
{

}

void PBRApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE TexTable0;
	TexTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE TexTable1;
	TexTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 12, 2, 0);

	CD3DX12_ROOT_PARAMETER SlotRootParameter[5];

	// Create root Constant Buffer Views (CBVs)
	// @todo Reorder from most frequent to least frequent.
	SlotRootParameter[0].InitAsConstantBufferView(0); // Per object CBV
	SlotRootParameter[1].InitAsConstantBufferView(1); // Per pass CBV
	SlotRootParameter[2].InitAsConstantBufferView(2); // Per material CBV
	SlotRootParameter[3].InitAsDescriptorTable(1, &TexTable0, D3D12_SHADER_VISIBILITY_PIXEL);
	SlotRootParameter[4].InitAsDescriptorTable(1, &TexTable1, D3D12_SHADER_VISIBILITY_PIXEL);

	auto StaticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(5, SlotRootParameter, 
		(uint) StaticSamplers.size(), StaticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*) errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	// Finally create the root signature
	ThrowIfFailed(m_D3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_RootSignature.GetAddressOf())));
}

void PBRApp::BuildCamera()
{
	m_Camera = Camera(M_PI_OVER_4, m_clientWidth, m_clientHeight, 0.1f, 100.0f);
	// Offset the camera's initial position by -5 in z
	float3 pos = {0.0f, 0.0f, -5.0f};
	vect vectPos = Vector::LoadFloat3(&pos);
	m_Camera.SetPosition(vectPos);
}

void PBRApp::BuildDescriptorHeaps()
{
	// Create SRV heap
	D3D12_DESCRIPTOR_HEAP_DESC SrvHeapDesc = {};
	// Number of textures in the heap
	int numTextures = 0;
	auto it = m_Textures.begin();
	// Loop through texture vectors
	while(it != m_Textures.end())
	{
		numTextures += it->second.size();
		it++;
	}
	SrvHeapDesc.NumDescriptors = numTextures;
	SrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	SrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_D3dDevice->CreateDescriptorHeap(&SrvHeapDesc, IID_PPV_ARGS(&m_SrvDescriptorHeap)));

	// Fill heap with actual descriptor
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
		m_SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc = { };
	SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SrvDesc.Texture2D.MostDetailedMip = 0;
	SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	int SRVHeapIndex = 0;
	it = m_Textures.begin();
	while(it != m_Textures.end())
	{
		for(int i = 0; i < it->second.size(); i++)
		{
			auto texture = it->second[i].get();
			if(texture != nullptr)
			{
				texture->SRVHeapIndex = SRVHeapIndex;
				D3D12_RESOURCE_DESC desc = texture->Resource->GetDesc();
				SrvDesc.Format = texture->Resource->GetDesc().Format;
				SrvDesc.Texture2D.MipLevels = texture->Resource->GetDesc().MipLevels;
				SrvDesc.Texture2D.MostDetailedMip = 0;
				SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
				m_D3dDevice->CreateShaderResourceView(texture->Resource.Get(), &SrvDesc, hDescriptor);
			}
			hDescriptor.Offset(1, m_cbvSrvDescriptorSize);
			SRVHeapIndex++;
		}
		it++;
	}
}

void PBRApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO OPAQUE_DEFINES[] =
	{
		"DIFFUSE_TEXTURE", "1",
		"SPECULAR_TEXTURE", "1",
		"METALLIC_TEXTURE", "1",
		"ROUGHNESS_TEXTURE", "1",
		"NORMAL_TEXTURE", "1",
		"DISPLACEMENT_TEXTURE", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO OPAQUE_ASRND_DEFINES[] =
	{
		"DIFFUSE_TEXTURE", "1",
		"SPECULAR_TEXTURE", "1",
		"ROUGHNESS_TEXTURE", "1",
		"NORMAL_TEXTURE", "1",
		"DISPLACEMENT_TEXTURE", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO OPAQUE_AMRN_DEFINES[] =
	{
		"DIFFUSE_TEXTURE", "1",
		"METALLIC_TEXTURE", "1",
		"ROUGHNESS_TEXTURE", "1",
		"NORMAL_TEXTURE", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO OPAQUE_TEXTURELESS_DEFINES[] =
	{
		NULL, NULL
	};

	const D3D_SHADER_MACRO ALPHA_TEST_DEFINES[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	m_Shaders["skyboxVS"] = d3dUtil::CompileShader(L"../../Shaders/Skybox.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["skyboxPS"] = d3dUtil::CompileShader(L"../../Shaders/Skybox.hlsl", nullptr, "PS", "ps_5_1");

	m_Shaders["standardVS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", nullptr, "VS", "vs_5_1");
	m_Shaders["opaquePS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", OPAQUE_DEFINES, "PS", "ps_5_1");
	m_Shaders["opaqueAmrnPS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", OPAQUE_AMRN_DEFINES, "PS", "ps_5_1");
	m_Shaders["opaqueAsrndPS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", OPAQUE_ASRND_DEFINES, "PS", "ps_5_1");
	m_Shaders["opaqueTexturelessPS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", OPAQUE_TEXTURELESS_DEFINES, "PS", "ps_5_1");

	m_Shaders["alphaTestedPS"] = d3dUtil::CompileShader(L"../../Shaders/Default.hlsl", ALPHA_TEST_DEFINES, "PS", "ps_5_1");
	m_InputLayout =
	{ 
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void PBRApp::BuildPSOs()
{
	// PSO for Opaque Objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaquePSODesc;
	ZeroMemory(&OpaquePSODesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	OpaquePSODesc.InputLayout = { m_InputLayout.data(), (uint) m_InputLayout.size() };
	OpaquePSODesc.pRootSignature = m_RootSignature.Get();
	OpaquePSODesc.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["standardVS"]->GetBufferPointer()),
		m_Shaders["standardVS"]->GetBufferSize()
	};
	OpaquePSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaquePS"]->GetBufferPointer()),
		m_Shaders["opaquePS"]->GetBufferSize()
	};
	OpaquePSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	OpaquePSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	OpaquePSODesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	OpaquePSODesc.SampleMask = UINT_MAX;
	OpaquePSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	OpaquePSODesc.NumRenderTargets = 1;
	OpaquePSODesc.RTVFormats[0] = m_BackBufferFormat;
	OpaquePSODesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
	OpaquePSODesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
	OpaquePSODesc.DSVFormat = m_DepthStencilFormat;
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&OpaquePSODesc, IID_PPV_ARGS(&m_PSOs["opaque"])));
	// PSO for opaque Asrnd objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaqueAsrndPSODesc = OpaquePSODesc;
	OpaqueAsrndPSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaqueAsrndPS"]->GetBufferPointer()),
		m_Shaders["opaqueAsrndPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&OpaqueAsrndPSODesc, IID_PPV_ARGS(&m_PSOs["opaqueAsrnd"])));
	// PSO for opaque Amrn objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaqueAmrnPSODesc = OpaquePSODesc;
	OpaqueAmrnPSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaqueAmrnPS"]->GetBufferPointer()),
		m_Shaders["opaqueAmrnPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&OpaqueAmrnPSODesc, IID_PPV_ARGS(&m_PSOs["opaqueAmrn"])));
	// PSO for Opaque Textureless Objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC OpaqueTexturelessPSODesc = OpaquePSODesc;
	OpaqueTexturelessPSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["opaqueTexturelessPS"]->GetBufferPointer()),
		m_Shaders["opaqueTexturelessPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&OpaqueTexturelessPSODesc, IID_PPV_ARGS(&m_PSOs["opaqueTextureless"])));
	
	// PSO for transparent objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC TransparentPSODesc = OpaquePSODesc;
	D3D12_RENDER_TARGET_BLEND_DESC TransparencyBlendDesc;
	TransparencyBlendDesc.BlendEnable = true;
	TransparencyBlendDesc.LogicOpEnable = false;
	TransparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	TransparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	TransparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	TransparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	TransparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	TransparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	TransparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	TransparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	TransparentPSODesc.BlendState.RenderTarget[0] = TransparencyBlendDesc;
	TransparentPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&TransparentPSODesc, IID_PPV_ARGS(&m_PSOs["transparent"])));

	// PSO for alpha tested objects
	D3D12_GRAPHICS_PIPELINE_STATE_DESC AlphaTestedPSODesc = OpaquePSODesc;
	AlphaTestedPSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["alphaTestedPS"]->GetBufferPointer()),
		m_Shaders["alphaTestedPS"]->GetBufferSize()
	};
	AlphaTestedPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&AlphaTestedPSODesc, IID_PPV_ARGS(&m_PSOs["alphaTested"])));

	// PSO for skybox
	D3D12_GRAPHICS_PIPELINE_STATE_DESC SkyBoxPSODesc = OpaquePSODesc;
	// The camera is inside the sky sphere, so just turn off culling.
	SkyBoxPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// Make sure the depth function is LESS_EQUAL and not just LESS.  
	// Otherwise, the normalized depth values at z = 1 (NDC) will 
	// fail the depth test if the depth buffer was cleared to 1.
	SkyBoxPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	SkyBoxPSODesc.pRootSignature = m_RootSignature.Get();
	SkyBoxPSODesc.VS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyboxVS"]->GetBufferPointer()),
		m_Shaders["skyboxVS"]->GetBufferSize()
	};
	SkyBoxPSODesc.PS =
	{
		reinterpret_cast<BYTE*>(m_Shaders["skyboxPS"]->GetBufferPointer()),
		m_Shaders["skyboxPS"]->GetBufferSize()
	};
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&SkyBoxPSODesc, IID_PPV_ARGS(&m_PSOs["skybox"])));

	// PSO for wireframe rendering
	D3D12_GRAPHICS_PIPELINE_STATE_DESC WireframePSODesc = OpaquePSODesc;
	WireframePSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(m_D3dDevice->CreateGraphicsPipelineState(&WireframePSODesc, IID_PPV_ARGS(&m_PSOs["opaque_wireframe"])));
}

void PBRApp::BuildFrameResources()
{
	for (int i = 0; i < NUM_FRAME_RESOURCES; i++)
	{
		m_FrameResources.push_back(std::make_unique<FrameResource>(m_D3dDevice.Get(),
			1, (uint) m_AllRenderItems.size(), (uint) m_Materials.size()));
	}
}

void PBRApp::BuildMaterials()
{
	auto skyBox = std::make_unique<Material>();
	skyBox->Name = "sky_box";
	skyBox->pDiffuse = m_Textures["sky_box"][0].get();
	m_Materials[skyBox->Name] = std::move(skyBox);

	auto rustSphere = std::make_unique<Material>();
	rustSphere->Name = "sphere_rust";	
	rustSphere->pDiffuse = m_Textures["rusted_iron"][0].get();
	rustSphere->pMetallic = m_Textures["rusted_iron"][2].get();
	rustSphere->pRoughness = m_Textures["rusted_iron"][3].get();
	rustSphere->pNormal = m_Textures["rusted_iron"][4].get();
	m_Materials[rustSphere->Name] = std::move(rustSphere);
	auto copperSphere = std::make_unique<Material>();
	copperSphere->Name = "sphere_rock_copper";
	copperSphere->pDiffuse = m_Textures["rock_copper"][0].get();
	copperSphere->pMetallic = m_Textures["rock_copper"][2].get();
	copperSphere->pRoughness = m_Textures["rock_copper"][3].get();
	copperSphere->pNormal = m_Textures["rock_copper"][4].get();
	m_Materials[copperSphere->Name] = std::move(copperSphere);
	auto brickSphere = std::make_unique<Material>();
	brickSphere->Name = "sphere_brick_modern";
	brickSphere->pDiffuse = m_Textures["brick_modern"][0].get();
	brickSphere->pSpecular = m_Textures["brick_modern"][1].get();
	brickSphere->pRoughness = m_Textures["brick_modern"][3].get();
	brickSphere->pNormal = m_Textures["brick_modern"][4].get();
	m_Materials[brickSphere->Name] = std::move(brickSphere);
	auto concreteDirtySphere = std::make_unique<Material>();
	concreteDirtySphere->Name = "sphere_concrete_dirty";
	concreteDirtySphere->pDiffuse = m_Textures["concrete_dirty"][0].get();
	concreteDirtySphere->pSpecular = m_Textures["concrete_dirty"][1].get();
	concreteDirtySphere->pRoughness = m_Textures["concrete_dirty"][3].get();
	concreteDirtySphere->pNormal = m_Textures["concrete_dirty"][4].get();
	m_Materials[concreteDirtySphere->Name] = std::move(concreteDirtySphere);
	auto concreteRoughSphere = std::make_unique<Material>();
	concreteRoughSphere->Name = "sphere_concrete_rough";
	concreteRoughSphere->pDiffuse = m_Textures["concrete_rough"][0].get();
	concreteRoughSphere->pSpecular = m_Textures["concrete_rough"][1].get();
	concreteRoughSphere->pRoughness = m_Textures["concrete_rough"][3].get();
	concreteRoughSphere->pNormal = m_Textures["concrete_rough"][4].get();
	m_Materials[concreteRoughSphere->Name] = std::move(concreteRoughSphere);
	auto grassSphere = std::make_unique<Material>();
	grassSphere->Name = "sphere_grass_wild";
	grassSphere->pDiffuse = m_Textures["grass_wild"][0].get();
	grassSphere->pSpecular = m_Textures["grass_wild"][1].get();
	grassSphere->pRoughness = m_Textures["grass_wild"][3].get();
	grassSphere->pNormal = m_Textures["grass_wild"][4].get();
	m_Materials[grassSphere->Name] = std::move(grassSphere);
	auto metalSphere = std::make_unique<Material>();
	metalSphere->Name = "sphere_metal_bare";
	metalSphere->pDiffuse = m_Textures["metal_bare"][0].get();
	metalSphere->pSpecular = m_Textures["metal_bare"][1].get();
	metalSphere->pMetallic = m_Textures["metal_bare"][2].get();
	metalSphere->pRoughness = m_Textures["metal_bare"][3].get();
	metalSphere->pNormal = m_Textures["metal_bare"][4].get();
	m_Materials[metalSphere->Name] = std::move(metalSphere);
	auto soilSphere = std::make_unique<Material>();
	soilSphere->Name = "sphere_soil_mud";
	soilSphere->pDiffuse = m_Textures["soil_mud"][0].get();
	soilSphere->pSpecular = m_Textures["soil_mud"][1].get();
	soilSphere->pRoughness = m_Textures["soil_mud"][3].get();
	soilSphere->pNormal = m_Textures["soil_mud"][4].get();
	m_Materials[soilSphere->Name] = std::move(soilSphere);
	auto stoneSphere = std::make_unique<Material>();
	stoneSphere->Name = "sphere_stone_wall";
	stoneSphere->pDiffuse = m_Textures["stone_wall"][0].get();
	stoneSphere->pSpecular = m_Textures["stone_wall"][1].get();
	stoneSphere->pRoughness = m_Textures["stone_wall"][3].get();
	stoneSphere->pNormal = m_Textures["stone_wall"][4].get();
	m_Materials[stoneSphere->Name] = std::move(stoneSphere);

	for(int i = 0; i < 49; i++)
	{
		auto redSphere = std::make_unique<Material>();
		redSphere->Name = "sphere_red_" + std::to_string(i);
		redSphere->Properties.Diffuse = {1.0f, 0.0f, 0.0f};
		redSphere->Properties.FresnelR0 = {0.04f, 0.04f, 0.04f};
		redSphere->Properties.Roughness = (i % 7) / 6.0f;
		redSphere->Properties.Metallic = 1.0f - (i / 7) / 6.0f;
		m_Materials[redSphere->Name] = std::move(redSphere);
	}
}

void PBRApp::BuildRenderItems()
{
	// Sky box
	auto skyRenderItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&skyRenderItem->World, XMMatrixScaling(5000.0f, 5000.0f, 5000.0f));
	skyRenderItem->TexTransform = MathHelper::Identity4x4();
	skyRenderItem->objCBIndex = 0;
	skyRenderItem->Mat = m_Materials["sky_box"].get();
	skyRenderItem->Geo = m_Meshes["sky_box"].get();
	skyRenderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRenderItem->indexCount = skyRenderItem->Geo->DrawArgs.begin()->second.IndexCount;
	skyRenderItem->startIndexLocation = skyRenderItem->Geo->DrawArgs.begin()->second.StartIndexLocation;
	skyRenderItem->baseVertexLocation = skyRenderItem->Geo->DrawArgs.begin()->second.BaseVertexLocation;
	m_RenderItemLayer[(int) RenderLayer::SkyBox].push_back(skyRenderItem.get());
	m_AllRenderItems.push_back(std::move(skyRenderItem));

	// All other render items
	auto mesh = m_Meshes.begin();
	int objCBIndex = 1;
	while(mesh != m_Meshes.end())
	{
		if(mesh->second->Name == "sky_box")
		{
			mesh++;
			continue;
		}

		auto submesh = mesh->second->DrawArgs.begin();
		while(submesh != mesh->second->DrawArgs.end())
		{
			auto renderItem = std::make_unique<RenderItem>();
			renderItem->objCBIndex = objCBIndex;
			renderItem->Mat = submesh->second.pMaterial;
			renderItem->Geo = mesh->second.get();
			renderItem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			renderItem->indexCount = submesh->second.IndexCount;
			renderItem->startIndexLocation = submesh->second.StartIndexLocation;
			renderItem->baseVertexLocation = submesh->second.BaseVertexLocation;

			// Hacky translation at render item build time
			if(renderItem->Mat->Name.substr(0, 11) == "sphere_red_")
			{
				int i = std::stoi(renderItem->Mat->Name.substr(11));
				float x = ((i%7) * 2.5f) - 3 * 2.5f;
				float y = ((i/7) * -2.5f) - 2.5f;
				vect4 world = Matrix::SetupTranslation(x, y, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_rust")
			{
				vect4 world = Matrix::SetupTranslation(0.0f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_rock_copper")
			{
				vect4 world = Matrix::SetupTranslation(-2.5f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_brick_modern")
			{
				vect4 world = Matrix::SetupTranslation(-5.0f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_concrete_dirty")
			{
				vect4 world = Matrix::SetupTranslation(-7.5f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_concrete_rough")
			{
				vect4 world = Matrix::SetupTranslation(-10.0f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_grass_wild")
			{
				vect4 world = Matrix::SetupTranslation(2.5f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_metal_bare")
			{
				vect4 world = Matrix::SetupTranslation(5.0f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_soil_mud")
			{
				vect4 world = Matrix::SetupTranslation(7.5f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}
			if(renderItem->Mat->Name == "sphere_stone_wall")
			{
				vect4 world = Matrix::SetupTranslation(10.0f, 0.0f, 0.0f);
				Matrix::StoreFloat4x4(&renderItem->World, world);
			}

			// Add to the appropriate renderlayer
			if(renderItem->Mat->pDiffuse == nullptr)
			{
				m_RenderItemLayer[(int) RenderLayer::OpaqueTextureless].push_back(renderItem.get());
			}
			else if(renderItem->Mat->pMetallic == nullptr)
			{ // @ todo missing metal texture
				m_RenderItemLayer[(int) RenderLayer::OpaqueAsrnd].push_back(renderItem.get());
			}
			else if(renderItem->Mat->pSpecular == nullptr)
			{ // @ todo missing spec texture
				m_RenderItemLayer[(int) RenderLayer::OpaqueAmrn].push_back(renderItem.get());
			}
			else
			{
				m_RenderItemLayer[(int) RenderLayer::Opaque].push_back(renderItem.get());
			}

			m_AllRenderItems.push_back(std::move(renderItem));
			submesh++;
		}
		objCBIndex++;
		mesh++;
	}
}

void PBRApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	uint objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	uint matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialProperties));

	auto ObjectCB = m_CurrFrameResource->ObjectCB->Resource();
	auto MatCB = m_CurrFrameResource->MaterialCB->Resource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE SkyTex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	SkyTex.Offset(m_Textures["sky_box"][0]->SRVHeapIndex, m_cbvSrvDescriptorSize );


	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);
	
		CD3DX12_GPU_DESCRIPTOR_HANDLE Tex(m_SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		if(ri->Mat->pDiffuse != nullptr)
			Tex.Offset(ri->Mat->pDiffuse->SRVHeapIndex, m_cbvSrvDescriptorSize);


		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = ObjectCB->GetGPUVirtualAddress()
			+ ri->objCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = MatCB->GetGPUVirtualAddress()
			+ ri->Mat->MatCBIndex*matCBByteSize;
		
		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(2, matCBAddress);
		cmdList->SetGraphicsRootDescriptorTable(3, SkyTex);
		cmdList->SetGraphicsRootDescriptorTable(4, Tex);


		cmdList->DrawIndexedInstanced(ri->indexCount, 1, ri->startIndexLocation, ri->baseVertexLocation, 0);
	}
}


std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> PBRApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return{
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp};
}


void PBRApp::LoadTextures()
{
	std::vector<std::unique_ptr<Texture>> skyBox(2);
	auto skyTex = std::make_unique<ImageTexture>();
	skyTex->Name = "sky_box";
	skyTex->Filename = L"../../../Assets/Subway_Lights/20_Subway_lights_3k.png";
	//skyTex->Filename = L"../../../Assets/Chelsea_Stairs/Chelsea_Stairs_3k.png";
	skyTex->Initialize(m_D3dDevice, m_CommandQueue);
	skyBox[0] = std::move(skyTex);
	skyTex = std::make_unique<ImageTexture>();
	skyTex->Name = "sky_env";
	skyTex->Filename = L"../../../Assets/Subway_Lights/20_Subway_lights_Env.png";
	//skyTex->Filename = L"../../../Assets/Chelsea_Stairs/Chelsea_Stairs_Env.png";
	skyTex->Initialize(m_D3dDevice, m_CommandQueue);
	skyBox[1] = std::move(skyTex);
	m_Textures["sky_box"] = std::move(skyBox);

	std::vector<std::unique_ptr<Texture>> redPixel(12);
	auto redPixelTex = std::make_unique<ImageTexture>();
	redPixelTex->Name = "pixel_red";
	redPixelTex->Filename = L"../../../Assets/RedPixel.png";
	redPixelTex->Initialize(m_D3dDevice, m_CommandQueue);
	redPixel[0] = std::move(redPixelTex);
	m_Textures["pixel_red"] = std::move(redPixel);

	std::vector<std::unique_ptr<Texture>> rockCopper(12);
	auto rcDiffuse = std::make_unique<ImageTexture>();
	rcDiffuse->Name = "rc_diffuse";
	rcDiffuse->Filename = L"../../../Assets/rockcopper/copper-rock1-alb.png";
	rcDiffuse->Initialize(m_D3dDevice, m_CommandQueue);
	auto rcMetalness = std::make_unique<ImageTexture>();
	rcMetalness->Name = "rc_metalness";
	rcMetalness->Filename = L"../../../Assets/rockcopper/copper-rock1-metal.png";
	rcMetalness->Initialize(m_D3dDevice, m_CommandQueue);
	auto rcRoughness = std::make_unique<ImageTexture>();
	rcRoughness->Name = "rc_roughness";
	rcRoughness->Filename = L"../../../Assets/rockcopper/copper-rock1-rough.png";
	rcRoughness->Initialize(m_D3dDevice, m_CommandQueue);
	auto rcNormal = std::make_unique<ImageTexture>();
	rcNormal->Name = "rc_normal";
	rcNormal->Filename = L"../../../Assets/rockcopper/copper-rock1-normal.png";
	rcNormal->Initialize(m_D3dDevice, m_CommandQueue);
	rockCopper[0] = std::move(rcDiffuse);
	rockCopper[2] = std::move(rcMetalness);
	rockCopper[3] = std::move(rcRoughness);
	rockCopper[4] = std::move(rcNormal);
	m_Textures["rock_copper"] = std::move(rockCopper);

	std::vector<std::unique_ptr<Texture>> rustedIron(12);
	auto riDiffuse = std::make_unique<ImageTexture>();
	riDiffuse->Name = "ri_diffuse";
	riDiffuse->Filename = L"../../../Assets/rustediron/rustediron2_basecolor.png";
	riDiffuse->Initialize(m_D3dDevice, m_CommandQueue);
	auto riMetalness = std::make_unique<ImageTexture>();
	riMetalness->Name = "ri_metalness";
	riMetalness->Filename = L"../../../Assets/rustediron/rustediron2_metallic.png";
	riMetalness->Initialize(m_D3dDevice, m_CommandQueue);
	auto riRoughness = std::make_unique<ImageTexture>();
	riRoughness->Name = "ri_roughness";
	riRoughness->Filename = L"../../../Assets/rustediron/rustediron2_roughness.png";
	riRoughness->Initialize(m_D3dDevice, m_CommandQueue);
	auto riNormal = std::make_unique<ImageTexture>();
	riNormal->Name = "ri_normal";
	riNormal->Filename = L"../../../Assets/rustediron/rustediron2_normal.png";
	riNormal->Initialize(m_D3dDevice, m_CommandQueue);
	rustedIron[0] = std::move(riDiffuse);
	rustedIron[2] = std::move(riMetalness);
	rustedIron[3] = std::move(riRoughness);
	rustedIron[4] = std::move(riNormal);
	m_Textures["rusted_iron"] = std::move(rustedIron);

	std::vector<std::unique_ptr<Texture>> brickModern1K(12);
	auto bm1K = std::make_unique<ImageTexture>();
	bm1K->Name = "bm1k_diffuse";
	bm1K->Filename = L"../../../Assets/Brick_Modern_1K/semlcibb_8K_Albedo.jpg";
	bm1K->Initialize(m_D3dDevice, m_CommandQueue);
	brickModern1K[0] = std::move(bm1K);
	bm1K = std::make_unique<ImageTexture>();
	bm1K->Name = "bm1k_specular";
	bm1K->Filename = L"../../../Assets/Brick_Modern_1K/semlcibb_8K_Specular.jpg";
	bm1K->Initialize(m_D3dDevice, m_CommandQueue);
	brickModern1K[1] = std::move(bm1K);
	bm1K = std::make_unique<ImageTexture>();
	bm1K->Name = "bm1k_roughness";
	bm1K->Filename = L"../../../Assets/Brick_Modern_1K/semlcibb_8K_Roughness.jpg";
	bm1K->Initialize(m_D3dDevice, m_CommandQueue);
	brickModern1K[3] = std::move(bm1K);
	bm1K = std::make_unique<ImageTexture>();
	bm1K->Name = "bm1k_normal";
	bm1K->Filename = L"../../../Assets/Brick_Modern_1K/semlcibb_8K_Normal.jpg";
	bm1K->Initialize(m_D3dDevice, m_CommandQueue);
	brickModern1K[4] = std::move(bm1K);
	bm1K = std::make_unique<ImageTexture>();
	bm1K->Name = "bm1k_displacement";
	bm1K->Filename = L"../../../Assets/Brick_Modern_1K/semlcibb_8K_Displacement.jpg";
	bm1K->Initialize(m_D3dDevice, m_CommandQueue);
	brickModern1K[5] = std::move(bm1K);
	m_Textures["brick_modern"] = std::move(brickModern1K);

	std::vector<std::unique_ptr<Texture>> concreteDirty1K(12);
	auto cd1K = std::make_unique<ImageTexture>();
	cd1K->Name = "cd1k_diffuse";
	cd1K->Filename = L"../../../Assets/Concrete_Dirty_1K/rm4kshp_4K_Albedo.jpg";
	cd1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteDirty1K[0] = std::move(cd1K);
	cd1K = std::make_unique<ImageTexture>();
	cd1K->Name = "cd1k_specular";
	cd1K->Filename = L"../../../Assets/Concrete_Dirty_1K/rm4kshp_4K_Specular.jpg";
	cd1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteDirty1K[1] = std::move(cd1K);
	cd1K = std::make_unique<ImageTexture>();
	cd1K->Name = "cd1k_roughness";
	cd1K->Filename = L"../../../Assets/Concrete_Dirty_1K/rm4kshp_4K_Roughness.jpg";
	cd1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteDirty1K[3] = std::move(cd1K);
	cd1K = std::make_unique<ImageTexture>();
	cd1K->Name = "cd1k_normal";
	cd1K->Filename = L"../../../Assets/Concrete_Dirty_1K/rm4kshp_4K_Normal.jpg";
	cd1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteDirty1K[4] = std::move(cd1K);
	cd1K = std::make_unique<ImageTexture>();
	cd1K->Name = "cd1k_displacement";
	cd1K->Filename = L"../../../Assets/Concrete_Dirty_1K/rm4kshp_4K_Displacement.jpg";
	cd1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteDirty1K[5] = std::move(cd1K);
	m_Textures["concrete_dirty"] = std::move(concreteDirty1K);

	std::vector<std::unique_ptr<Texture>> concreteRough1K(12);
	auto cr1K = std::make_unique<ImageTexture>();
	cr1K->Name = "cr1k_diffuse";
	cr1K->Filename = L"../../../Assets/Concrete_Rough_1K/sdbhdd3b_8K_Albedo.jpg";
	cr1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteRough1K[0] = std::move(cr1K);
	cr1K = std::make_unique<ImageTexture>();
	cr1K->Name = "cr1k_specular";
	cr1K->Filename = L"../../../Assets/Concrete_Rough_1K/sdbhdd3b_8K_Specular.jpg";
	cr1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteRough1K[1] = std::move(cr1K);
	cr1K = std::make_unique<ImageTexture>();
	cr1K->Name = "cr1k_roughness";
	cr1K->Filename = L"../../../Assets/Concrete_Rough_1K/sdbhdd3b_8K_Roughness.jpg";
	cr1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteRough1K[3] = std::move(cr1K);
	cr1K = std::make_unique<ImageTexture>();
	cr1K->Name = "cr1k_normal";
	cr1K->Filename = L"../../../Assets/Concrete_Rough_1K/sdbhdd3b_8K_Normal.jpg";
	cr1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteRough1K[4] = std::move(cr1K);
	cr1K = std::make_unique<ImageTexture>();
	cr1K->Name = "cr1k_displacement";
	cr1K->Filename = L"../../../Assets/Concrete_Rough_1K/sdbhdd3b_8K_Displacement.jpg";
	cr1K->Initialize(m_D3dDevice, m_CommandQueue);
	concreteRough1K[5] = std::move(cr1K);
	m_Textures["concrete_rough"] = std::move(concreteRough1K);

	std::vector<std::unique_ptr<Texture>> grassWild1K(12);
	auto gw1K = std::make_unique<ImageTexture>();
	gw1K->Name = "gw1k_diffuse";
	gw1K->Filename = L"../../../Assets/Grass_Wild_1K/sfknaeoa_8K_Albedo.jpg";
	gw1K->Initialize(m_D3dDevice, m_CommandQueue);
	grassWild1K[0] = std::move(gw1K);
	gw1K = std::make_unique<ImageTexture>();
	gw1K->Name = "gw1k_specular";
	gw1K->Filename = L"../../../Assets/Grass_Wild_1K/sfknaeoa_8K_Specular.jpg";
	gw1K->Initialize(m_D3dDevice, m_CommandQueue);
	grassWild1K[1] = std::move(gw1K);
	gw1K = std::make_unique<ImageTexture>();
	gw1K->Name = "gw1k_roughness";
	gw1K->Filename = L"../../../Assets/Grass_Wild_1K/sfknaeoa_8K_Roughness.jpg";
	gw1K->Initialize(m_D3dDevice, m_CommandQueue);
	grassWild1K[3] = std::move(gw1K);
	gw1K = std::make_unique<ImageTexture>();
	gw1K->Name = "gw1k_normal";
	gw1K->Filename = L"../../../Assets/Grass_Wild_1K/sfknaeoa_8K_Normal.jpg";
	gw1K->Initialize(m_D3dDevice, m_CommandQueue);
	grassWild1K[4] = std::move(gw1K);
	gw1K = std::make_unique<ImageTexture>();
	gw1K->Name = "gw1k_displacement";
	gw1K->Filename = L"../../../Assets/Grass_Wild_1K/sfknaeoa_8K_Displacement.jpg";
	gw1K->Initialize(m_D3dDevice, m_CommandQueue);
	grassWild1K[5] = std::move(gw1K);
	m_Textures["grass_wild"] = std::move(grassWild1K);

	std::vector<std::unique_ptr<Texture>> metalBare1K(12);
	auto mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_diffuse";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Albedo.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[0] = std::move(mb1K);
	mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_specular";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Specular.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[1] = std::move(mb1K);
	mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_metallic";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Metalness.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[2] = std::move(mb1K);
	mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_roughness";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Roughness.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[3] = std::move(mb1K);
	mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_normal";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Normal.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[4] = std::move(mb1K);
	mb1K = std::make_unique<ImageTexture>();
	mb1K->Name = "mb1k_displacement";
	mb1K->Filename = L"../../../Assets/Metal_Bare_1K/se2abbvc_8K_Displacement.jpg";
	mb1K->Initialize(m_D3dDevice, m_CommandQueue);
	metalBare1K[5] = std::move(mb1K);
	m_Textures["metal_bare"] = std::move(metalBare1K);

	std::vector<std::unique_ptr<Texture>> soilMud1K(12);
	auto sm1K = std::make_unique<ImageTexture>();
	sm1K->Name = "sm1k_diffuse";
	sm1K->Filename = L"../../../Assets/Soil_Mud_1K/pjDtB2_8K_Albedo.jpg";
	sm1K->Initialize(m_D3dDevice, m_CommandQueue);
	soilMud1K[0] = std::move(sm1K);
	sm1K = std::make_unique<ImageTexture>();
	sm1K->Name = "sm1k_specular";
	sm1K->Filename = L"../../../Assets/Soil_Mud_1K/pjDtB2_8K_Specular.jpg";
	sm1K->Initialize(m_D3dDevice, m_CommandQueue);
	soilMud1K[1] = std::move(sm1K);
	sm1K = std::make_unique<ImageTexture>();
	sm1K->Name = "sm1k_roughness";
	sm1K->Filename = L"../../../Assets/Soil_Mud_1K/pjDtB2_8K_Roughness.jpg";
	sm1K->Initialize(m_D3dDevice, m_CommandQueue);
	soilMud1K[3] = std::move(sm1K);
	sm1K = std::make_unique<ImageTexture>();
	sm1K->Name = "sm1k_normal";
	sm1K->Filename = L"../../../Assets/Soil_Mud_1K/pjDtB2_8K_Normal.jpg";
	sm1K->Initialize(m_D3dDevice, m_CommandQueue);
	soilMud1K[4] = std::move(sm1K);
	sm1K = std::make_unique<ImageTexture>();
	sm1K->Name = "sm1k_displacement";
	sm1K->Filename = L"../../../Assets/Soil_Mud_1K/pjDtB2_8K_Displacement.jpg";
	sm1K->Initialize(m_D3dDevice, m_CommandQueue);
	soilMud1K[5] = std::move(sm1K);
	m_Textures["soil_mud"] = std::move(soilMud1K);

	std::vector<std::unique_ptr<Texture>> stoneWall1K(12);
	auto sw1K = std::make_unique<ImageTexture>();
	sw1K->Name = "sw1k_diffuse";
	sw1K->Filename = L"../../../Assets/Stone_Wall_1K/scpgdgca_8K_Albedo.jpg";
	sw1K->Initialize(m_D3dDevice, m_CommandQueue);
	stoneWall1K[0] = std::move(sw1K);
	sw1K = std::make_unique<ImageTexture>();
	sw1K->Name = "sw1k_specular";
	sw1K->Filename = L"../../../Assets/Stone_Wall_1K/scpgdgca_8K_Specular.jpg";
	sw1K->Initialize(m_D3dDevice, m_CommandQueue);
	stoneWall1K[1] = std::move(sw1K);
	sw1K = std::make_unique<ImageTexture>();
	sw1K->Name = "sw1k_roughness";
	sw1K->Filename = L"../../../Assets/Stone_Wall_1K/scpgdgca_8K_Roughness.jpg";
	sw1K->Initialize(m_D3dDevice, m_CommandQueue);
	stoneWall1K[3] = std::move(sw1K);
	sw1K = std::make_unique<ImageTexture>();
	sw1K->Name = "sw1k_normal";
	sw1K->Filename = L"../../../Assets/Stone_Wall_1K/scpgdgca_8K_Normal.jpg";
	sw1K->Initialize(m_D3dDevice, m_CommandQueue);
	stoneWall1K[4] = std::move(sw1K);
	sw1K = std::make_unique<ImageTexture>();
	sw1K->Name = "sw1k_displacement";
	sw1K->Filename = L"../../../Assets/Stone_Wall_1K/scpgdgca_8K_Displacement.jpg";
	sw1K->Initialize(m_D3dDevice, m_CommandQueue);
	stoneWall1K[5] = std::move(sw1K);
	m_Textures["stone_wall"] = std::move(stoneWall1K);
}



void PBRApp::LoadOBJModel(std::string filepath)
{
	// Load model, or throw error
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	std::string basepath = filepath.substr(0, filepath.find_last_of("/") + 1);
	if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filepath.c_str(), basepath.c_str(), true))
	{
		throw std::runtime_error(err);
	}

	// Create new Mesh
	auto mesh = std::make_unique<PolygonalMesh>();

	// Set top level Model name
	std::string objName = filepath.substr(filepath.find_last_of("/") + 1); // get local file path
	objName = objName.substr(0, objName.size() - 4);  // remove .obj file extension
	mesh->Name = objName;

	std::vector<Vertex> Vertices = std::vector<Vertex>();
	uint lastIndexCount = 0;
	// Assume 32 bit indices
	uint maxIndex = 0;
	std::unordered_map<Vertex, u32> uniqueVertices = { };
	std::vector<u32> Indices = std::vector<u32>();
	// Iterate over .obj geometries
	for(const auto& shape : shapes)
	{
		Submesh submesh;
		submesh.Name = shape.name;

		// Get the material that defines this shape's visuals
		if(shape.mesh.material_ids[0] != -1)
		{
			// Add new shape Material to m_material
			tinyobj::material_t tinyobjShapeMat = materials[shape.mesh.material_ids[0]];
			std::string matName = objName + "::";
			matName = matName + tinyobjShapeMat.name;
			if(m_Materials.find(matName) == m_Materials.end())
			{ // new material
				auto shapeMat = std::make_unique<Material>();
				// @todo deal with different illum models in tinyobjShapeMat
				shapeMat->Properties.Anisotropy = tinyobjShapeMat.anisotropy;
				shapeMat->Properties.AnisotropyRotation = tinyobjShapeMat.anisotropy_rotation;
				shapeMat->Properties.ClearCoatRoughness = tinyobjShapeMat.clearcoat_roughness;
				shapeMat->Properties.ClearCoatThickness = tinyobjShapeMat.clearcoat_thickness;
				shapeMat->Properties.Diffuse = Vector::SetFloat3(tinyobjShapeMat.diffuse[0], tinyobjShapeMat.diffuse[1], tinyobjShapeMat.diffuse[2]);
				shapeMat->Properties.Emissive = Vector::SetFloat3(tinyobjShapeMat.emission[0], tinyobjShapeMat.emission[1], tinyobjShapeMat.emission[2]);
				shapeMat->Properties.FresnelR0 = Vector::SetFloat3(tinyobjShapeMat.specular[0], tinyobjShapeMat.specular[1], tinyobjShapeMat.specular[2]);
				shapeMat->Properties.Metallic = tinyobjShapeMat.metallic;
				shapeMat->Properties.Opacity = tinyobjShapeMat.dissolve;
				shapeMat->Properties.Roughness = 1 - Math::Min(tinyobjShapeMat.shininess / 256.0f, 1.0f); // @todo needs be switched to roughness for real pbr
				shapeMat->Properties.Sheen = tinyobjShapeMat.sheen;
				shapeMat->Properties.Transmission = Vector::SetFloat3(tinyobjShapeMat.transmittance[0], tinyobjShapeMat.transmittance[1], tinyobjShapeMat.transmittance[2]);

				if(tinyobjShapeMat.diffuse_texname != "")
					AddImageTexture(shapeMat->pDiffuse, basepath, tinyobjShapeMat.diffuse_texname); // Contains a diffuse texture, load it

				//shapeMat->SetCBIndex();

				// Move new material into m_Materials, and assign it the new shape
				m_Materials[matName] = std::move(shapeMat);
				submesh.pMaterial = m_Materials[matName].get();
			}
		}

		// Get shape vertices and indices
		for(const auto& index : shape.mesh.indices)
		{
			Vertex V;
			V.Pos = Vector::SetFloat3(
				attrib.vertices[3*index.vertex_index+0],
				attrib.vertices[3*index.vertex_index+1],
				attrib.vertices[3*index.vertex_index+2]
			);
			if(index.normal_index!=-1)
			{
				V.Normal = Vector::SetFloat3(
					attrib.normals[3*index.normal_index+0],
					attrib.normals[3*index.normal_index+1],
					attrib.normals[3*index.normal_index+2]
				);
			}
			else
			{
				V.Normal = Vector::SetFloat3(0.0f, 0.0f, 0.0f);
			}
			if(index.texcoord_index!=-1)
			{
				V.TexCoord = Vector::SetFloat2(
					attrib.texcoords[2*index.texcoord_index+0],
					1.0f-attrib.texcoords[2*index.texcoord_index+1]
				);
			}
			else
			{
				V.TexCoord = Vector::SetFloat2(0.0f, 0.0f);
			}

			if(uniqueVertices.count(V)==0)
			{ // V is not in uniqueVertices
				uniqueVertices[V] = static_cast<u32>(Vertices.size());
				maxIndex = (uniqueVertices[V] > maxIndex) ? uniqueVertices[V] : maxIndex;
				Vertices.push_back(V);
			}

			Indices.push_back(uniqueVertices[V]);
		}

		submesh.StartIndexLocation = lastIndexCount;
		submesh.IndexCount = Indices.size() - lastIndexCount;
		lastIndexCount = Indices.size();

		mesh->DrawArgs[submesh.Name] = submesh;
	}

	std::vector<u16> Indices16 = std::vector<u16>();
	uint indexTypeSize = sizeof(u32);
	// Convert to 16 bit indices if there are few enough
	if(maxIndex<=0xFFFF)
	{ // Convert to 16 bit indices
		Indices16.resize(Indices.size());
		for(uint i = 0; i < Indices.size(); i++)
		{
			Indices16[i] = static_cast<u16>(Indices[i]);
		}
		indexTypeSize = sizeof(u16);
		mesh->IndexFormat = DXGI_FORMAT_R16_UINT;
	}

	const uint vbByteSize = (uint) Vertices.size()*sizeof(Vertex);
	const uint ibByteSize = (uint) Indices.size() * indexTypeSize;

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mesh->VertexBufferCPU));
	CopyMemory(mesh->VertexBufferCPU->GetBufferPointer(), Vertices.data(), vbByteSize);

	if(maxIndex<=0xFFFF)
	{
		ThrowIfFailed(D3DCreateBlob(ibByteSize, &mesh->IndexBufferCPU));
		CopyMemory(mesh->IndexBufferCPU->GetBufferPointer(), Indices16.data(), ibByteSize);
	}
	else
	{
		ThrowIfFailed(D3DCreateBlob(ibByteSize, &mesh->IndexBufferCPU));
		CopyMemory(mesh->IndexBufferCPU->GetBufferPointer(), Indices.data(), ibByteSize);
	}

	mesh->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(m_D3dDevice.Get(),
		m_CommandList.Get(), Vertices.data(), vbByteSize, mesh->VertexBufferUploader);

	if(maxIndex<=0xFFFF)
	{
		mesh->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_D3dDevice.Get(),
			m_CommandList.Get(), Indices16.data(), ibByteSize, mesh->IndexBufferUploader);
	}
	else
	{
		mesh->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(m_D3dDevice.Get(),
			m_CommandList.Get(), Indices.data(), ibByteSize, mesh->IndexBufferUploader);
	}

	mesh->VertexByteStride = sizeof(Vertex);
	mesh->VertexBufferByteSize = vbByteSize;
	mesh->IndexBufferByteSize = ibByteSize;

	m_Meshes[mesh->Name] = std::move(mesh);
}

void PBRApp::AddImageTexture(Texture *&pTexture, std::string basepath, std::string texName)
{
	/*
	std::string filepath = basepath + texName;
	if(m_Textures.find(filepath) == m_Textures.end())
	{   // new texture
		auto tex = std::make_unique<ImageTexture>();
		std::wstring wsTmp(filepath.begin(), filepath.end()); // @error will only work for UTF-8 encoding
		tex->Filename = wsTmp;
		tex->Name = texName; //@error folder delimiter assumed to be '/'
		tex->Initialize(m_D3dDevice, m_CommandQueue);
		m_Textures[filepath] = std::move(tex);
	}
	pTexture = m_Textures[filepath].get();
	*/
}
