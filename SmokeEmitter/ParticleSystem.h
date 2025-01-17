#pragma once
#include "Random.h"
#include "SimpleCamera.h"
#include "ParticleSystemHelper.h"
#include <vector>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace ParticleSystem
{
	extern D3D12_INPUT_ELEMENT_DESC InputDescription[];
	extern UINT InputDescriptionNumElements;
	extern const UINT TexturePixelSize;
};

struct ParticleProps
{
	XMFLOAT3 Position;
	XMFLOAT3 PositionDistribution;
	XMFLOAT3 Velocity;
	XMFLOAT3 VelocityDistribution;
	XMFLOAT3 Force;
	XMFLOAT4 ColorBegin;
	XMFLOAT4 ColorEnd;

	float SizeBegin;
	float SizeEnd;
	float SizeDistribution;
	float LifeTime;
};

struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 uv;
};

struct InstanceData {
	XMFLOAT4 color;
	XMFLOAT4X4 model;
	FLOAT life;
};

struct Particle
{
	XMVECTOR Position;
	XMVECTOR Velocity;
	XMVECTOR Force;

	float Rotation = 0.0f;
	float SizeBegin = 1.0f;
	float SizeEnd = 1.0f;
	//float SizeBegin, SizeEnd, SizeVariation;

	XMVECTOR ColorBegin, ColorEnd;
	float LifeTime = 1.0f;
	float LifeRemaining = 0.0f;
	bool Active = false;
};

struct PP
{
	XMFLOAT4 Position;
	XMFLOAT4 PositionDistribution;
	XMFLOAT4 Velocity;
	XMFLOAT4 VelocityDistribution;
	XMFLOAT4 Force;
	XMFLOAT4 ColorBegin;
	XMFLOAT4 ColorEnd;

	float SizeBegin, SizeEnd;
	float SizeDistribution;
	float LifeTime;
};


class Emitter
{
public:
	Emitter();
	void OnInit(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize);

	void OnUpdate(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize, float elapsedSeconds);
	void OnRender(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize);

	void Emit(const ParticleProps& particleProps);
	
	void CompileShaders(ComPtr<ID3D12Device>& device);
	void CreatePipelineState(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& rootSignature);

	void CreateDescriptor(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize);
	void AllocateDataOnGPU(ComPtr<ID3D12Device>& device);
	void UploadShaderResource(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& commandList);

	// app resources
	ComPtr<ID3D12Resource> m_texture_smoke_A;
	ComPtr<ID3D12Resource> m_texture_smoke_F;
	ComPtr<ID3D12Resource> m_texture_smoke_N;
	ComPtr<ID3D12Resource> m_texture_circle;

	ComPtr<ID3D12Resource> m_particlesPool;
	ComPtr<ID3D12Resource> m_aliveLists[2];
	ComPtr<ID3D12Resource> m_deadList;
	ComPtr<ID3D12Resource> m_counters;
	ComPtr<ID3D12Resource> m_sortedAliveList;
	UINT8* m_pParticleDataBegin;
	// compute shader
	ComPtr<ID3DBlob> initCS;
	ComPtr<ID3DBlob> emitCS;
	ComPtr<ID3DBlob> updateCS;
	ComPtr<ID3DBlob> initSortCS;
	ComPtr<ID3DBlob> sortCS;
	ComPtr<ID3DBlob> renderCS;
	ComPtr<ID3DBlob> finishCS;

	ComPtr<ID3DBlob> billboardVS;
	ComPtr<ID3DBlob> billboardPS;
	// pipeline state object
	ComPtr<ID3D12PipelineState> m_initPSO;
	ComPtr<ID3D12PipelineState> m_emitPSO;
	ComPtr<ID3D12PipelineState> m_updatePSO;
	ComPtr<ID3D12PipelineState> m_initSortPSO;
	ComPtr<ID3D12PipelineState> m_sortPSO;
	ComPtr<ID3D12PipelineState> m_renderPSO;
	ComPtr<ID3D12PipelineState> m_finishPSO;

	ComPtr<ID3D12PipelineState> m_particleDrawPSO;

	// Create the GPU upload buffer.
	ComPtr<ID3D12Resource> m_textureUploadHeap;
	// alive slot (for swap)
	int m_aliveSlots[2] = { 3, 4 };

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_instanceBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
	UINT8* m_pInstanceDataBegin;

	float spawnRate = 3.0f;

	static const UINT POOL_MAX = 1024;
	static const UINT POOL_SIZE = 1024;
private:
	Vertex m_triangleVertices[6] =
	{
		{ { 0.25f, 0.25f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 1.0f, 1.0f, 0.0f } },
		{ { -0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
		{ { -0.25f, 0.25f, 0.0f }, { 0.0f, 0.0f, 0.0f } },
		{ { 0.25f, -0.25f, 0.0f }, { 1.0f, 1.0f, 0.0f } },
		{ { -0.25f, -0.25f, 0.0f }, { 0.0f, 1.0f, 0.0f } }
	};

	InstanceData m_particleInstances[POOL_MAX];

	float lastEmitElapsedSeconds = 0.0f;
	//GLuint m_QuadVA = 0;
	//std::unique_ptr<GLCore::Utils::Shader> m_ParticleShader;
	//GLint m_ParticleShaderViewProj, m_ParticleShaderTransform, m_ParticleShaderColor;
};