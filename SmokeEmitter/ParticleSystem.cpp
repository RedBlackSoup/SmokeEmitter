#include "stdafx.h"
#include "ParticleSystem.h"
#include <cassert>
#include <algorithm>
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif

#include "stb_image.h"

std::wstring GetAssetFullPath(LPCWSTR assetName)
{
	return assetName;
}

namespace ParticleSystem
{
	D3D12_INPUT_ELEMENT_DESC InputDescription[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODEL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODEL", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODEL", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "MODEL", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "LIFE", 0, DXGI_FORMAT_R32_FLOAT, 1, 80,D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
	};

	UINT InputDescriptionNumElements = _countof(InputDescription);

	const UINT TexturePixelSize = 4;
}

std::vector<UINT8> LoadTextureData(std::string filePath, UINT textureWidth, UINT textureHeight)
{
	const UINT rowPitch = textureWidth * ParticleSystem::TexturePixelSize;
	const UINT textureSize = rowPitch * textureHeight;

	std::vector<UINT8> data(textureSize);
	UINT8* pData = &data[0];

	int width, height, channels;
	UINT8* imageData = stbi_load(filePath.c_str(), &width, &height, &channels, 4); // 读取图片，4表示RGBA格式
	if (!imageData) {
		// 处理错误
		std::cout << "Failed to load image: " << stbi_failure_reason() << std::endl;
	}

	memcpy(pData, imageData, textureSize);
	stbi_image_free(imageData);

	return data;
}

Emitter::Emitter()
{


} 

void Emitter::OnInit(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize)
{
	// 设置计算着色器根签名
	commandList->SetComputeRootSignature(rootSignature.Get());
	commandList->SetPipelineState(m_initPSO.Get());

	// 绑定 UAV 描述符堆到计算着色器
	ID3D12DescriptorHeap* ppHeaps[] = { cbvHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// 绑定到计算着色器根参数
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(cbvHeap->GetGPUDescriptorHandleForHeapStart(), 0, descriptorSize);
	commandList->SetComputeRootDescriptorTable(0, cbvSrvUavHandle); // Contant buffer
	cbvSrvUavHandle.Offset(descriptorSize * 4);
	commandList->SetComputeRootDescriptorTable(2, cbvSrvUavHandle); // Particle Buffer
	cbvSrvUavHandle.Offset(descriptorSize);
	commandList->SetComputeRootDescriptorTable(m_aliveSlots[0], cbvSrvUavHandle); // Alivelist[0]
	cbvSrvUavHandle.Offset(descriptorSize);
	commandList->SetComputeRootDescriptorTable(m_aliveSlots[1], cbvSrvUavHandle); // AliveList[1]
	cbvSrvUavHandle.Offset(descriptorSize);
	commandList->SetComputeRootDescriptorTable(5, cbvSrvUavHandle); // DeadList | InstanceData | Counters | Sorted alive list

	// 执行计算着色器
	commandList->Dispatch(1, 1, 1);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_deadList.Get()));
}

void Emitter::OnUpdate(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12RootSignature>& rootSignature, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize, float elapsedSeconds)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_particlesPool.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS
	));

	// 绑定到计算着色器根参数
	{
		commandList->SetComputeRootSignature(rootSignature.Get());

		CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(cbvHeap->GetGPUDescriptorHandleForHeapStart(), 0, descriptorSize);
		commandList->SetComputeRootDescriptorTable(0, cbvSrvUavHandle); // Constant buffer
		cbvSrvUavHandle.Offset(descriptorSize * 4);
		commandList->SetComputeRootDescriptorTable(2, cbvSrvUavHandle); // Particle Buffer
		cbvSrvUavHandle.Offset(descriptorSize);
		commandList->SetComputeRootDescriptorTable(m_aliveSlots[0], cbvSrvUavHandle); // Alivelist[0]
		cbvSrvUavHandle.Offset(descriptorSize);
		commandList->SetComputeRootDescriptorTable(m_aliveSlots[1], cbvSrvUavHandle); // AliveList[1]
		cbvSrvUavHandle.Offset(descriptorSize);
		commandList->SetComputeRootDescriptorTable(5, cbvSrvUavHandle); // DeadList | InstanceData | Counters | SortedAliveList
		cbvSrvUavHandle.Offset(descriptorSize * 4);
		commandList->SetComputeRootDescriptorTable(6, cbvSrvUavHandle); // DepthBuffer
	}

	D3D12_RESOURCE_BARRIER barriers[7];
	// 对每个 UAV 资源设置资源屏障
	barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(m_particlesPool.Get());
	barriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(m_aliveLists[0].Get());
	barriers[2] = CD3DX12_RESOURCE_BARRIER::UAV(m_aliveLists[1].Get());
	barriers[3] = CD3DX12_RESOURCE_BARRIER::UAV(m_deadList.Get());
	barriers[4] = CD3DX12_RESOURCE_BARRIER::UAV(m_instanceBuffer.Get());
	barriers[5] = CD3DX12_RESOURCE_BARRIER::UAV(m_counters.Get());
	barriers[6] = CD3DX12_RESOURCE_BARRIER::UAV(m_sortedAliveList.Get());

	float perParticleSeconds = 1.0f / spawnRate;
	lastEmitElapsedSeconds += elapsedSeconds;
	if(lastEmitElapsedSeconds > perParticleSeconds)
	{
		commandList->SetPipelineState(m_emitPSO.Get());
		commandList->Dispatch(1, 1, 1);

		commandList->ResourceBarrier(7, barriers);
		lastEmitElapsedSeconds -= perParticleSeconds;
	}
	
	commandList->SetPipelineState(m_updatePSO.Get());
	commandList->Dispatch(1024, 1, 1);

	commandList->ResourceBarrier(7, barriers);

	for (int wid = 1; wid < POOL_SIZE; wid *= 2)
	{
		commandList->SetPipelineState(m_initSortPSO.Get());
		commandList->Dispatch(1, 1, 1);
		commandList->ResourceBarrier(7, barriers);
		commandList->SetPipelineState(m_sortPSO.Get());
		commandList->Dispatch(1024, 1, 1);
		commandList->ResourceBarrier(7, barriers);
	}

	commandList->SetPipelineState(m_renderPSO.Get());
	commandList->Dispatch(1024, 1, 1);

	commandList->ResourceBarrier(7, barriers);

	commandList->SetPipelineState(m_finishPSO.Get());
	commandList->Dispatch(1, 1, 1);

	commandList->ResourceBarrier(7, barriers);

	// 转换为 Pixel Shader 可读
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_particlesPool.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
	));

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_instanceBuffer.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_GENERIC_READ
	));

	std::swap(m_aliveSlots[0], m_aliveSlots[1]);
}

void Emitter::OnRender(ComPtr<ID3D12GraphicsCommandList>& commandList, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize)
{
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_instanceBuffer.Get(),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
	));

	// Set necessary state.
	CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(cbvHeap->GetGPUDescriptorHandleForHeapStart(), 1, descriptorSize);
	commandList->SetGraphicsRootDescriptorTable(1, cbvSrvUavHandle); // textrues
	cbvSrvUavHandle.Offset(descriptorSize * 10);
	commandList->SetGraphicsRootDescriptorTable(6, cbvSrvUavHandle); // DepthBuffer

	// Draw particles
	commandList->SetPipelineState(m_particleDrawPSO.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetVertexBuffers(1, 1, &m_instanceBufferView);
	commandList->DrawInstanced(6, 1024, 0, 0);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		m_instanceBuffer.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_GENERIC_READ
	));
}

void Emitter::Emit(const ParticleProps& particleProps)
{

}

void Emitter::CompileShaders(ComPtr<ID3D12Device>& device)
{
#if defined(_DEBUG)
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif
	ComPtr<ID3DBlob> errors;

	D3DCompileFromFile(GetAssetFullPath(L"particle_initCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &initCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_emitCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &emitCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_updateCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &updateCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_initSortCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &initSortCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_sortCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &sortCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_renderCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &renderCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"particle_finishCS.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMain", "cs_5_0", compileFlags, 0, &finishCS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &billboardVS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();

	D3DCompileFromFile(GetAssetFullPath(L"shaders.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &billboardPS, &errors);
	if (errors != nullptr)
	{
		OutputDebugStringA((char*)errors->GetBufferPointer());
	}
	errors.Reset();
}

void Emitter::CreatePipelineState(ComPtr<ID3D12Device>& device, ComPtr<ID3D12RootSignature>& rootSignature)
{
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC initPSODesc = {};
		initPSODesc.pRootSignature = rootSignature.Get();
		initPSODesc.CS = CD3DX12_SHADER_BYTECODE(initCS.Get());
		initPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&initPSODesc, IID_PPV_ARGS(&m_initPSO));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC emitPSODesc = {};
		emitPSODesc.pRootSignature = rootSignature.Get();
		emitPSODesc.CS = CD3DX12_SHADER_BYTECODE(emitCS.Get());
		emitPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&emitPSODesc, IID_PPV_ARGS(&m_emitPSO));
	}

	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC updatePSODesc = {};
		updatePSODesc.pRootSignature = rootSignature.Get();
		updatePSODesc.CS = CD3DX12_SHADER_BYTECODE(updateCS.Get());
		updatePSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&updatePSODesc, IID_PPV_ARGS(&m_updatePSO));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC renderPSODesc = {};
		renderPSODesc.pRootSignature = rootSignature.Get();
		renderPSODesc.CS = CD3DX12_SHADER_BYTECODE(sortCS.Get());
		renderPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&renderPSODesc, IID_PPV_ARGS(&m_sortPSO));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC renderPSODesc = {};
		renderPSODesc.pRootSignature = rootSignature.Get();
		renderPSODesc.CS = CD3DX12_SHADER_BYTECODE(initSortCS.Get());
		renderPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&renderPSODesc, IID_PPV_ARGS(&m_initSortPSO));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC renderPSODesc = {};
		renderPSODesc.pRootSignature = rootSignature.Get();
		renderPSODesc.CS = CD3DX12_SHADER_BYTECODE(renderCS.Get());
		renderPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&renderPSODesc, IID_PPV_ARGS(&m_renderPSO));
	}
	{
		D3D12_COMPUTE_PIPELINE_STATE_DESC finishPSODesc = {};
		finishPSODesc.pRootSignature = rootSignature.Get();
		finishPSODesc.CS = CD3DX12_SHADER_BYTECODE(finishCS.Get());
		finishPSODesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		device->CreateComputePipelineState(&finishPSODesc, IID_PPV_ARGS(&m_finishPSO));
	}
	{
		// Describe and create the blend state.
		D3D12_BLEND_DESC blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE; // 关闭 Alpha 到覆盖
		blendDesc.IndependentBlendEnable = FALSE; // 不启用独立混合

		// 颜色的混合将基于源颜色的 alpha 值，而 alpha 的混合将完全基于源的 alpha 值，从而影响最终的透明效果。
		blendDesc.RenderTarget[0].BlendEnable = TRUE; // 启用混合
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA; // 源的 Alpha 值
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // 目标的反向 Alpha 值
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD; // 混合操作

		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE; // Alpha 源为 1
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO; // Alpha 目标为 0
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD; // Alpha 混合操作
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 写入所有颜色通道

		// 深度/模板状态
		D3D12_DEPTH_STENCIL_DESC depthStencilDescTransparent = {};
		depthStencilDescTransparent.DepthEnable = TRUE; // 启用深度测试
		depthStencilDescTransparent.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 禁止写入深度缓存
		depthStencilDescTransparent.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 深度比较：小于
		depthStencilDescTransparent.StencilEnable = FALSE; // 禁用模板测试

		// Describe and create the graphics pipeline state object (PSO).
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { ParticleSystem::InputDescription, ParticleSystem::InputDescriptionNumElements };
		psoDesc.pRootSignature = rootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(billboardVS.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(billboardPS.Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = blendDesc;

		psoDesc.DepthStencilState = depthStencilDescTransparent;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
		psoDesc.SampleDesc.Count = 1;

		device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_particleDrawPSO));
	}
}

void Emitter::CreateDescriptor(ComPtr<ID3D12Device>& device, ComPtr<ID3D12DescriptorHeap>& cbvHeap, UINT descriptorSize)
{
	// Textures.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 1, descriptorSize);    // Move past the CBV in slot 1.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(m_texture_smoke_A.Get(), &srvDesc, srvHandle);
	}
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 2, descriptorSize);    // Move past the CBV in slot 2.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(m_texture_smoke_F.Get(), &srvDesc, srvHandle);
	}
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 3, descriptorSize);    // Move past the CBV in slot 3.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(m_texture_smoke_N.Get(), &srvDesc, srvHandle);
	}
	// Initialize the structured buffer view.
	// Particle pool
	{
		const UINT SIZE_PARTICLE = 104;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = SIZE_PARTICLE;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 4, descriptorSize);
		device->CreateUnorderedAccessView(m_particlesPool.Get(), nullptr, &uavDesc, uavHandle);
	}

	// Alive lists 0
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 5, descriptorSize);
		device->CreateUnorderedAccessView(m_aliveLists[0].Get(), nullptr, &uavDesc, uavHandle);
	}

	// Alive lists 1
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 6, descriptorSize);
		device->CreateUnorderedAccessView(m_aliveLists[1].Get(), nullptr, &uavDesc, uavHandle);
	}

	// Dead list
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 7, descriptorSize);
		device->CreateUnorderedAccessView(m_deadList.Get(), nullptr, &uavDesc, uavHandle);
	}

	// Instance buffer
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = sizeof(InstanceData);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 8, descriptorSize);
		device->CreateUnorderedAccessView(m_instanceBuffer.Get(), nullptr, &uavDesc, uavHandle);
	}

	// Counters
	{
		const UINT ReservedBufferSize = 16;

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = ReservedBufferSize;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 9, descriptorSize);
		device->CreateUnorderedAccessView(m_counters.Get(), nullptr, &uavDesc, uavHandle);
	}

	// Sorted alive list
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = POOL_SIZE;
		uavDesc.Buffer.StructureByteStride = sizeof(UINT);
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
	
		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 10, descriptorSize);
		device->CreateUnorderedAccessView(m_sortedAliveList.Get(), nullptr, &uavDesc, uavHandle);
	}
	// Circle texture.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(cbvHeap->GetCPUDescriptorHandleForHeapStart(), 12, descriptorSize);    // Move past the CBV in slot 1.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(m_texture_circle.Get(), &srvDesc, srvHandle);
	}

}

void Emitter::AllocateDataOnGPU(ComPtr<ID3D12Device>& device)
{	
	// Create billboard vertex buffer
	{
		const UINT vertexBufferSize = sizeof(Vertex) * 6;

		// Note: using upload heaps to transfer static data like vert buffers is not 
		// recommended. Every time the GPU needs it, the upload heap will be marshalled 
		// over. Please read up on Default Heap usage. An upload heap is used here for 
		// code simplicity and because there are very few verts to actually transfer.
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
		memcpy(pVertexDataBegin, m_triangleVertices, sizeof(m_triangleVertices));
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = vertexBufferSize;
	}
	
	// Create the instance buffer.
	{
		const UINT instanceBufferSize = sizeof(InstanceData) * POOL_SIZE;

		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Width = instanceBufferSize; // 缓冲区大小
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc.Count = 1;
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		// 使用默认堆创建（GPU优化堆，只能从GPU访问）
		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			nullptr,
			IID_PPV_ARGS(&m_instanceBuffer)
		);

		// Initialize the vertex buffer view.
		m_instanceBufferView.BufferLocation = m_instanceBuffer->GetGPUVirtualAddress();
		m_instanceBufferView.StrideInBytes = sizeof(InstanceData);
		m_instanceBufferView.SizeInBytes = instanceBufferSize;
	}

	// Create the texture smoke_A
	{
		// Describe and create a smoke texture.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = 2048;
		textureDesc.Height = 2048;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture_smoke_A));
	}
	// Create the texture smoke_F
	{
		// Describe and create a smoke texture.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = 2048;
		textureDesc.Height = 2048;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture_smoke_F));
	}
	// Create the texture smoke_N
	{
		// Describe and create a smoke texture.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = 1024;
		textureDesc.Height = 1024;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture_smoke_N));
	}
	// Create the texture circle
	{
		// Describe and create a circle texture.
		D3D12_RESOURCE_DESC textureDesc = {};
		textureDesc.MipLevels = 1;
		textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureDesc.Width = 2048;
		textureDesc.Height = 2048;
		textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		textureDesc.DepthOrArraySize = 1;
		textureDesc.SampleDesc.Count = 1;
		textureDesc.SampleDesc.Quality = 0;
		textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

		device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&textureDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_texture_circle));
	}
	// Create the structured buffer (dead list).
	{
		const UINT SIZE_PARTICLE = 104;

		// Create the particle pool.
		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = SIZE_PARTICLE * POOL_SIZE;
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			// 为结构化缓冲区创建默认资源
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_particlesPool)
			);
		}
		// Create the alive lists & dead list && sorted alive list.
		{
			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = sizeof(INT) * POOL_SIZE;
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			// 为结构化缓冲区创建默认资源
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_aliveLists[0])
			);

			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_aliveLists[1])
			);

			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_deadList)
			);

			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_sortedAliveList)
			);
		}
		// Create the counter buffer.
		{
			const UINT ReservedBufferSize = 16;

			D3D12_RESOURCE_DESC bufferDesc = {};
			bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			bufferDesc.Width = sizeof(UINT) * ReservedBufferSize;
			bufferDesc.Height = 1;
			bufferDesc.DepthOrArraySize = 1;
			bufferDesc.MipLevels = 1;
			bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
			bufferDesc.SampleDesc.Count = 1;
			bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			// 为结构化缓冲区创建默认资源
			device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&m_counters)
			);
		}
	}
}

void Emitter::UploadShaderResource(ComPtr<ID3D12Device>& device, ComPtr<ID3D12GraphicsCommandList>& commandList)
{
	// Create the GPU upload buffer.
	const UINT64 uploadBufferSize1 = GetRequiredIntermediateSize(m_texture_smoke_A.Get(), 0, 1);
	const UINT64 uploadBufferSize2 = GetRequiredIntermediateSize(m_texture_smoke_F.Get(), 0, 1);
	const UINT64 uploadBufferSize3 = GetRequiredIntermediateSize(m_texture_smoke_N.Get(), 0, 1);
	const UINT64 uploadBufferSize4 = GetRequiredIntermediateSize(m_texture_circle.Get(), 0, 1);


	const UINT64 totalUploadBufferSize = uploadBufferSize1 + uploadBufferSize2 + uploadBufferSize3 + uploadBufferSize4;

	device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(totalUploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_textureUploadHeap));

	// Copy data to the intermediate upload heap and then schedule a copy from the upload heap to the Texture2D.
	std::vector<UINT8> texture_smoke_A = LoadTextureData("T_Smoke_A.png", 2048, 2048);
	std::vector<UINT8> texture_smoke_F = LoadTextureData("T_Smoke_F.png", 2048, 2048);
	std::vector<UINT8> texture_smoke_N = LoadTextureData("T_Smoke_N.png", 1024, 1024);
	std::vector<UINT8> texture_circle = LoadTextureData("circle_image.png", 2048, 2048);


	D3D12_SUBRESOURCE_DATA textureDataA = {};
	textureDataA.pData = &texture_smoke_A[0];
	textureDataA.RowPitch = 2048 * ParticleSystem::TexturePixelSize;
	textureDataA.SlicePitch = textureDataA.RowPitch * 2048;

	D3D12_SUBRESOURCE_DATA textureDataF = {};
	textureDataF.pData = &texture_smoke_F[0];
	textureDataF.RowPitch = 2048 * ParticleSystem::TexturePixelSize;
	textureDataF.SlicePitch = textureDataF.RowPitch * 2048;

	D3D12_SUBRESOURCE_DATA textureDataN = {};
	textureDataN.pData = &texture_smoke_N[0];
	textureDataN.RowPitch = 1024 * ParticleSystem::TexturePixelSize;
	textureDataN.SlicePitch = textureDataN.RowPitch * 1024;

	D3D12_SUBRESOURCE_DATA textureDataCircle = {};
	textureDataCircle.pData = &texture_circle[0];
	textureDataCircle.RowPitch = 2048 * ParticleSystem::TexturePixelSize;
	textureDataCircle.SlicePitch = textureDataCircle.RowPitch * 2048;

	UpdateSubresources(commandList.Get(), m_texture_smoke_A.Get(), m_textureUploadHeap.Get(), 0, 0, 1, &textureDataA);
	UpdateSubresources(commandList.Get(), m_texture_smoke_F.Get(), m_textureUploadHeap.Get(), uploadBufferSize1, 0, 1, &textureDataF);
	UpdateSubresources(commandList.Get(), m_texture_smoke_N.Get(), m_textureUploadHeap.Get(), uploadBufferSize1 + uploadBufferSize2, 0, 1, &textureDataN);
	UpdateSubresources(commandList.Get(), m_texture_circle.Get(), m_textureUploadHeap.Get(), uploadBufferSize1 + uploadBufferSize2 + uploadBufferSize3, 0, 1, &textureDataCircle);

	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture_smoke_A.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture_smoke_F.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture_smoke_N.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
	commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_texture_circle.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

}
