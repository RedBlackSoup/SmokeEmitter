//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "stdafx.h"
#include "SmokeEmitterDemo.h"

SmokeEmitterDemo::SmokeEmitterDemo(UINT width, UINT height, std::wstring name) :
    DXSample(width, height, name),
    m_frameIndex(0),
    m_frameCounter(0),
    m_pCbvDataBegin(nullptr),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_rtvDescriptorSize(0),
    m_constantBufferData{}
{
}

void SmokeEmitterDemo::OnInit()
{
    m_camera.Init({ 0, 0, 5 });
    m_constantBufferData.Lightings[0] = { {0.0f, 5.0f, 20.0f}, 1.0f, {0.2f, 0.9f, 0.3f, 1.0f} };
    m_constantBufferData.Lightings[1] = {{6.0f, 8.0f, 6.0f}, 3.5f, {0.2f, 0.9f, 0.3f, 1.0f}};
    m_constantBufferData.DirLights[0] = { {-0.2f, -1.0f, -0.5f}, 0.4f, {0.2f, 0.9f, 0.3f, 1.0f} };
    m_constantBufferData.DirLights[1] = { {-1.2f, 1.0f, 0.0f}, 0.9f, {0.2f, 0.9f, 0.3f, 1.0f}};

    LoadPipeline();
    LoadAssets();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NoKeyboard; // Disable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(Win32Application::GetHwnd());

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 60, m_cbvSrvUavDescriptorSize);
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 60, m_cbvSrvUavDescriptorSize);

    ImGui_ImplDX12_Init(m_device.Get(), FrameCount,
        DXGI_FORMAT_R8G8B8A8_UNORM, m_cbvSrvUavHeap.Get(),
        cpuHandle,
        gpuHandle
    );

    auto& pp = m_constantBufferData.particleProps;
    pp.Position = { 0.0f, 0.0f, 0.0f, 0.0f };
    pp.PositionDistribution = { 2.0f, 1.5f, 0.5f, 1.0 };
    pp.Velocity = { 0.0f, 0.0f, 0.0f, 0.0 };
    pp.VelocityDistribution = { 3.0f, 0.0f, 3.0f, 0.0f };
    pp.Force = { 0.0f, 0.1f, 0.0f, 0.0f };
    pp.ColorBegin = { 224.6 / 255.0f, 215 / 255.0f, 224.5/ 255.0f, 1.0f };
    pp.ColorEnd = { 224.6 / 255.0f, 208.8 / 255.0f, 220.5 / 255.0f, 1.0f };
    pp.SizeBegin = 3.0f;
    pp.SizeEnd = 5.0f;
    pp.SizeDistribution = 0.3f;
    pp.LifeTime = 10.0f;
}

// Load the rendering pipeline dependencies.
void SmokeEmitterDemo::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
            ));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
        ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a depth stencil view (DSV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        // Describe and create a shader resource view (SRV) and constant buffer view (CBV) descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
        cbvSrvUavHeapDesc.NumDescriptors = 64;
        cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)));

        m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV and a command allocator for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);

            ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        }
    }
}

// Load the sample assets.
void SmokeEmitterDemo::LoadAssets()
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[7];
        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 3, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        ranges[5].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
        ranges[6].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 3, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

        CD3DX12_ROOT_PARAMETER1 rootParameters[7];
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL); 
        rootParameters[3].InitAsDescriptorTable(1, &ranges[3], D3D12_SHADER_VISIBILITY_ALL); // 配置 UAV 描述符表，仅对计算着色器可见
        rootParameters[4].InitAsDescriptorTable(1, &ranges[4], D3D12_SHADER_VISIBILITY_ALL); // 配置 UAV 描述符表，仅对计算着色器可见
        rootParameters[5].InitAsDescriptorTable(1, &ranges[5], D3D12_SHADER_VISIBILITY_ALL); // 配置 UAV 描述符表，仅对计算着色器可见
        rootParameters[6].InitAsDescriptorTable(1, &ranges[6], D3D12_SHADER_VISIBILITY_ALL); // 配置 depth buffer, 用于Collision

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &sampler, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        //if (error != nullptr)
        //{
        //    OutputDebugStringA((char*)error->GetBufferPointer());
        //}
    }

    // Create the opaque graphic pipeline state.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        ComPtr<ID3DBlob> errors;

        D3DCompileFromFile(GetAssetFullPath(L"model_opaque.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, &errors);
        if (errors != nullptr)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
            printf("%s", errors->GetBufferPointer());
        }
        errors.Reset();

        D3DCompileFromFile(GetAssetFullPath(L"model_opaque.hlsl").c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, &errors);
        if (errors != nullptr)
        {
            OutputDebugStringA((char*)errors->GetBufferPointer());
            printf("%s", errors->GetBufferPointer());
        }
        errors.Reset();

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.InputLayout = { Scene::StandardVertexDescription, Scene::StandardVertexDescriptionNumElements };
        psoDesc.InputLayout = { Scene::StandardVertexDescription, Scene::StandardVertexDescriptionNumElements};
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);;
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleDesc.Count = 1;

        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_opaquePSO)));
    }
    m_emitter.CompileShaders(m_device);
    m_emitter.CreatePipelineState(m_device, m_rootSignature);

    m_emitter.AllocateDataOnGPU(m_device);
    m_emitter.CreateDescriptor(m_device, m_cbvSrvUavHeap, m_cbvSrvUavDescriptorSize);

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_emitter.m_particleDrawPSO.Get(), IID_PPV_ARGS(&m_commandList)));
    
    LoadScenes();

    // Create the shared vertex buffer.
    {
        const UINT vertexBufferSize = sizeof(Vertex) * m_sceneVertices.size();

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_sharedVertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_sharedVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, m_sceneVertices.data(), vertexBufferSize);
        m_sharedVertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_sharedVertexBufferView.BufferLocation = m_sharedVertexBuffer->GetGPUVirtualAddress();
        m_sharedVertexBufferView.StrideInBytes = sizeof(Vertex);
        m_sharedVertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create the index buffer.
    {
        //const UINT indexBufferSize = sizeof(Vertex) * 6 * 1000;

        //ThrowIfFailed(m_device->CreateCommittedResource(
        //    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        //    D3D12_HEAP_FLAG_NONE,
        //    &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        //    D3D12_RESOURCE_STATE_COPY_DEST,
        //    nullptr,
        //    IID_PPV_ARGS(&m_indexBuffer)));

        //ThrowIfFailed(m_device->CreateCommittedResource(
        //    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        //    D3D12_HEAP_FLAG_NONE,
        //    &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
        //    D3D12_RESOURCE_STATE_GENERIC_READ,
        //    nullptr,
        //    IID_PPV_ARGS(&indexBufferUploadHeap)));

        //NAME_D3D12_OBJECT(m_indexBuffer);

        //// Copy data to the intermediate upload heap and then schedule a copy 
        //// from the upload heap to the index buffer.
        //D3D12_SUBRESOURCE_DATA indexData = {};
        //indexData.pData = pMeshData + SampleAssets::IndexDataOffset;
        //indexData.RowPitch = SampleAssets::IndexDataSize;
        //indexData.SlicePitch = indexData.RowPitch;

        //UpdateSubresources<1>(m_commandList.Get(), m_indexBuffer.Get(), indexBufferUploadHeap.Get(), 0, 0, 1, &indexData);
        //m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_indexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER));

        //// Describe the index buffer view.
        //m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
        //m_indexBufferView.Format = SampleAssets::StandardIndexFormat;
        //m_indexBufferView.SizeInBytes = SampleAssets::IndexDataSize;

        //m_numIndices = SampleAssets::IndexDataSize / 4;    // R32_UINT (SampleAssets::StandardIndexFormat) = 4 bytes each.
    }

    // Create the constant buffer.
    {
        const UINT constantBufferSize = sizeof(SceneConstantBuffer);    // CB size is required to be 256-byte aligned.

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constantBuffer)));

        // Describe and create a constant buffer view.
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;
        m_device->CreateConstantBufferView(&cbvDesc, m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart());

        // Map and initialize the constant buffer. We don't unmap this until the
        // app closes. Keeping things mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin)));
        memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    }

    // Create the depth stencil view.
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        NAME_D3D12_OBJECT(m_depthStencil);

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Create the depth shader resource view
    {
        // 创建 SRV 描述
        D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc = {};
        depthSrvDesc.Format = DXGI_FORMAT_R32_FLOAT; // 将深度值转换为浮点格式
        depthSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        depthSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        depthSrvDesc.Texture2D.MipLevels = 1;

        // 创建 SRV
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), 11, m_cbvSrvUavDescriptorSize);
        m_device->CreateShaderResourceView(m_depthStencil.Get(), &depthSrvDesc, srvHandle);
    }
    m_emitter.UploadShaderResource(m_device, m_commandList);
    // Close the command list and execute it to begin the initial GPU setup.
    m_emitter.OnInit(m_commandList, m_rootSignature, m_cbvSrvUavHeap, m_cbvSrvUavDescriptorSize);
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    ThrowIfFailed(m_commandList->Close());
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValues[m_frameIndex]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForGpu();
    }
}

// Update frame-based values.
void SmokeEmitterDemo::OnUpdate()
{
    m_timer.Tick(NULL);

    if (m_frameCounter == 500)
    {
        // Update window text with FPS value.
        wchar_t fps[64];
        swprintf_s(fps, L"%ufps", m_timer.GetFramesPerSecond());
        SetCustomWindowText(fps);
        m_frameCounter = 0;
    }
    m_frameCounter++;

    XMFLOAT3 viewPos = m_camera.GetPosition();
    XMFLOAT3 upDirection = m_camera.GetUpDirection(); // t
    XMFLOAT3 lookDirection = m_camera.GetLookDirection();

    XMMATRIX view = m_camera.GetViewMatrix();
    XMMATRIX proj = m_camera.GetProjectionMatrix(XM_PI / 3.0f, m_aspectRatio);

    XMMATRIX viewInv = XMMatrixInverse(nullptr, view);
    XMMATRIX projectionInv = XMMatrixInverse(nullptr, proj);

    XMStoreFloat4x4(&m_constantBufferData.ViewProjInv, XMMatrixTranspose(projectionInv * viewInv));
    XMStoreFloat4x4(&m_constantBufferData.View, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_constantBufferData.ViewProj, XMMatrixTranspose(view * proj));

    m_constantBufferData.ViewPos = { viewPos.x, viewPos.y, viewPos.z, 1.0 };
    m_constantBufferData.UpDirection = { upDirection.x, upDirection.y, upDirection.z, 1.0};
    m_constantBufferData.lookDirection = lookDirection;

    m_constantBufferData.elapsedSeconds = m_timer.GetElapsedSeconds();


    float theta = 1.0f * m_timer.GetTotalSeconds();
    m_constantBufferData.Lightings[1].Postion = {sin(theta) * 6.0f, 8.0f, cos(theta) * 6.0f};


    for (size_t i = 0; i < 20; i++)
    {
        m_constantBufferData.randomBuffer[i] = Random::Float();
    }
    
    m_constantBufferData.ScreenWidth = m_width;
    m_constantBufferData.ScreenHeight = m_height;


    memcpy(m_pCbvDataBegin, &m_constantBufferData, sizeof(m_constantBufferData));
    
    m_camera.Update(static_cast<float>(m_timer.GetElapsedSeconds()));
}

// Render the scene.
void SmokeEmitterDemo::OnRender()
{
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
    {
        auto& particle = m_constantBufferData.particleProps;
        static int counter = 0;
        ImGui::Begin("Smoke Emitter");                          // Create a window called "Hello, world!" and append into it.

        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::ColorEdit4("color begin", (float*)&particle.ColorBegin);
        ImGui::ColorEdit4("color end", (float*)&particle.ColorEnd);

        ImGui::SliderFloat2("size", &particle.SizeBegin, 0.0f, 5.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::SliderFloat3("shape(theta, radius, height)", (float*)&particle.PositionDistribution, 1.0f, 20.0f);
        ImGui::InputFloat3("velocity", (float*)&particle.Velocity);
        ImGui::InputFloat3("velocity distribution", (float*)&particle.VelocityDistribution);

        ImGui::SliderFloat("life time", &particle.LifeTime, 1.0f, 20.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::SliderFloat3("force", (float*) &particle.Force, -10.0f, 10.0f);

        ImGui::SliderFloat("spwan rate", &m_emitter.spawnRate, 0.1f, 100.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        
        ImGui::ColorEdit3("clear color", (float*)&m_clearColor); // Edit 3 floats representing a color

        //if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
        //    counter++;
        //ImGui::SameLine();
        //ImGui::Text("counter = %d", counter);

        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::End();
    }

    // Rendering
    ImGui::Render();

    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

void SmokeEmitterDemo::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGpu();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CloseHandle(m_fenceEvent);
}

void SmokeEmitterDemo::OnKeyDown(UINT8 key)
{
    m_camera.OnKeyDown(key);
}

void SmokeEmitterDemo::OnKeyUp(UINT8 key)
{
    m_camera.OnKeyUp(key);
}

void SmokeEmitterDemo::OnResize(UINT width, UINT height)
{
    if (m_device != nullptr)
    {
        //WaitForGpu();
        //for (UINT i = 0; i < FrameCount; i++)
        //    if (m_renderTargets[i].Get()) { m_renderTargets[i]->Release();}

        //HRESULT result = m_swapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        //assert(SUCCEEDED(result) && "Failed to resize swapchain.");
        //
        //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        //// Create a RTV and a command allocator for each frame.
        //for (UINT n = 0; n < FrameCount; n++)
        //{
        //    ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        //    m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        //    rtvHandle.Offset(1, m_rtvDescriptorSize);

        //    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n])));
        //}
    }
}

// Fill the command list with all the render commands and dependent state.
void SmokeEmitterDemo::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.

    ThrowIfFailed(m_commandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_opaquePSO.Get()));
    
    // Set necessary state
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
    ));

    m_emitter.OnUpdate(m_commandList, m_rootSignature, m_cbvSrvUavHeap, m_cbvSrvUavDescriptorSize, m_timer.GetElapsedSeconds());

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    ));

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    // Set common constant buffer
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_cbvSrvUavDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(0, cbvSrvUavHandle);

    // draw scene object
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    // Record commands.

    m_commandList->ClearRenderTargetView(rtvHandle, m_clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    sceneObjectDraw();

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        D3D12_RESOURCE_STATE_DEPTH_READ
    ));


    // Draw particles
    m_emitter.OnRender(m_commandList, m_cbvSrvUavHeap, m_cbvSrvUavDescriptorSize);
    
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_depthStencil.Get(),
        D3D12_RESOURCE_STATE_DEPTH_READ,
        D3D12_RESOURCE_STATE_DEPTH_WRITE
    ));

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
    
}

void SmokeEmitterDemo::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void SmokeEmitterDemo::WaitForGpu()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_fenceValues[m_frameIndex]++;
}

void SmokeEmitterDemo::LoadScenes()
{
    // Position | Normal
    Vertex sceneVertices[12] =
    {
        { { 25.0f, -1.0f, 25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -25.0f, -1.0f, 25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { 25.0f, -1.0f, -25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { 25.0f, -1.0f, -25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -25.0f, -1.0f, 25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -25.0f, -1.0f, -25.0f }, { 0.0f, 1.0f, 0.0f } },
        { { -25.0f, 0.0f, -25.0f }, { 1.0f, -1.0f, 0.0f } },
        { { -25.0f, 0.0f, 25.0f }, { 1.0f, -1.0f, 0.0f } },
        { { 25.0f, 25.0f, 25.0f }, { 1.0f, -1.0f, 0.0f } },
        { { -25.0f, 0.0f, -25.0f }, { 1.0f, -1.0f, 0.0f } },
        { { 25.0f, 25.0f, 25.0f }, { 1.0f, -1.0f, 0.0f } },
        { { 25.0f, 25.0f, -25.0f }, { 1.0f, -1.0f, 0.0f } }
    };

    
    m_sceneVertices.resize(12);

    for (int i = 0; i < 12; i++)
    {
        m_sceneVertices[i] = sceneVertices[i];
    }
}

void SmokeEmitterDemo::sceneObjectDraw()
{
    // Draw scene object
    m_commandList->SetPipelineState(m_opaquePSO.Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_sharedVertexBufferView);
    //m_commandList->IASetVertexBuffers(1, 1, &m_instanceBufferView);
    m_commandList->DrawInstanced(m_sceneVertices.size(), 1, 0, 0);
}

//std::vector<UINT8> SmokeEmitterDemo::LoadTextureData(std::string filePath, UINT textureWidth, UINT textureHeight)
//{
//    const UINT rowPitch = textureWidth * TexturePixelSize;
//    const UINT textureSize = rowPitch * textureHeight;
//
//    std::vector<UINT8> data(textureSize);
//    UINT8* pData = &data[0];
//
//    int width, height, channels;
//    UINT8* imageData = stbi_load(filePath.c_str(), &width, &height, &channels, 4); // 读取图片，4表示RGBA格式
//    if (!imageData) {
//        // 处理错误
//        std::cout << "Failed to load image: " << stbi_failure_reason() << std::endl;
//    }
//
//    memcpy(pData, imageData, textureSize);
//    stbi_image_free(imageData);
//
//    return data;
//}
//void SmokeEmitterDemo::particleSimulate()
//{
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_particlesPool.Get(),
//        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
//        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
//    ));
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_instanceBuffer.Get(),
//        D3D12_RESOURCE_STATE_GENERIC_READ,
//        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
//    ));
//    
//    // 绑定到计算着色器根参数
//    {
//        m_commandList->SetComputeRootSignature(m_rootSignature.Get());
//
//        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_cbvSrvUavDescriptorSize);
//        m_commandList->SetComputeRootDescriptorTable(0, cbvSrvUavHandle); // Contant buffer
//        cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize * 4);
//        m_commandList->SetComputeRootDescriptorTable(2, cbvSrvUavHandle); // Particle Buffer
//        cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//        m_commandList->SetComputeRootDescriptorTable(m_aliveSlots[0], cbvSrvUavHandle); // Alivelist[0]
//        cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//        m_commandList->SetComputeRootDescriptorTable(m_aliveSlots[1], cbvSrvUavHandle); // AliveList[1]
//        cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//        m_commandList->SetComputeRootDescriptorTable(5, cbvSrvUavHandle); // DeadList | InstanceData | Counters | SortedAliveList
//        cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize * 4);
//        m_commandList->SetComputeRootDescriptorTable(6, cbvSrvUavHandle); // DepthBuffer
//    }
//
//    D3D12_RESOURCE_BARRIER barriers[7];
//    // 对每个 UAV 资源设置资源屏障
//    barriers[0] = CD3DX12_RESOURCE_BARRIER::UAV(m_particlesPool.Get());
//    barriers[1] = CD3DX12_RESOURCE_BARRIER::UAV(m_aliveLists[0].Get());
//    barriers[2] = CD3DX12_RESOURCE_BARRIER::UAV(m_aliveLists[1].Get());
//    barriers[3] = CD3DX12_RESOURCE_BARRIER::UAV(m_deadList.Get());
//    barriers[4] = CD3DX12_RESOURCE_BARRIER::UAV(m_instanceBuffer.Get());
//    barriers[5] = CD3DX12_RESOURCE_BARRIER::UAV(m_counters.Get());
//    barriers[6] = CD3DX12_RESOURCE_BARRIER::UAV(m_sortedAliveList.Get());
//
//    m_commandList->SetPipelineState(m_emitPSO.Get());
//    m_commandList->Dispatch(1, 1, 1);
//    
//    m_commandList->ResourceBarrier(7, barriers);
//    
//    m_commandList->SetPipelineState(m_updatePSO.Get());
//    m_commandList->Dispatch(1024, 1, 1);
//
//    m_commandList->ResourceBarrier(7, barriers);
//
//
//    for (int wid = 1; wid < ParticlePoolSize; wid *= 2)
//    {
//        m_commandList->SetPipelineState(m_initSortPSO.Get());
//        m_commandList->Dispatch(1, 1, 1);
//        m_commandList->ResourceBarrier(7, barriers);
//        m_commandList->SetPipelineState(m_sortPSO.Get());
//        m_commandList->Dispatch(1024, 1, 1);
//        m_commandList->ResourceBarrier(7, barriers);
//    }
//
//    m_commandList->SetPipelineState(m_renderPSO.Get());
//    m_commandList->Dispatch(1024, 1, 1);
//
//    m_commandList->ResourceBarrier(7, barriers);
//
//    m_commandList->SetPipelineState(m_finishPSO.Get());
//    m_commandList->Dispatch(1, 1, 1);
//
//    m_commandList->ResourceBarrier(7, barriers);
//    
//    // 转换为 Pixel Shader 可读
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_particlesPool.Get(),
//        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
//        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
//    ));
//
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_instanceBuffer.Get(),
//        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
//        D3D12_RESOURCE_STATE_GENERIC_READ
//    ));
//
//    std::swap(m_aliveSlots[0], m_aliveSlots[1]);
//
//    return;
//}
//
//void SmokeEmitterDemo::particleDraw()
//{
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_instanceBuffer.Get(),
//        D3D12_RESOURCE_STATE_GENERIC_READ,
//        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER
//    ));
//
//    // Set necessary state.
//    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_cbvSrvUavDescriptorSize);
//    m_commandList->SetGraphicsRootDescriptorTable(1, cbvSrvUavHandle); // textrues
//
//    // Draw particles
//    m_commandList->SetPipelineState(m_transparentPSO.Get());
//    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
//    m_commandList->IASetVertexBuffers(1, 1, &m_instanceBufferView);
//    m_commandList->DrawInstanced(6, 1024, 0, 0);
//
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
//        m_instanceBuffer.Get(),
//        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
//        D3D12_RESOURCE_STATE_GENERIC_READ
//    ));
//}
//
//void SmokeEmitterDemo::InitGpuResource()
//{
//    // 设置计算着色器根签名
//    m_commandList->SetComputeRootSignature(m_rootSignature.Get());
//    m_commandList->SetPipelineState(m_initPSO.Get());
//
//    // 绑定 UAV 描述符堆到计算着色器
//    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get() };
//    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
//
//    // 绑定到计算着色器根参数
//    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvUavHandle(m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_cbvSrvUavDescriptorSize);
//    m_commandList->SetComputeRootDescriptorTable(0, cbvSrvUavHandle); // Contant buffer
//    cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize * 4);
//    m_commandList->SetComputeRootDescriptorTable(2, cbvSrvUavHandle); // Particle Buffer
//    cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//    m_commandList->SetComputeRootDescriptorTable(m_aliveSlots[0], cbvSrvUavHandle); // Alivelist[0]
//    cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//    m_commandList->SetComputeRootDescriptorTable(m_aliveSlots[1], cbvSrvUavHandle); // AliveList[1]
//    cbvSrvUavHandle.Offset(m_cbvSrvUavDescriptorSize);
//    m_commandList->SetComputeRootDescriptorTable(5, cbvSrvUavHandle); // DeadList | InstanceData | Counters | Sorted alive list
//    
//    // 执行计算着色器
//    m_commandList->Dispatch(1, 1, 1);
//
//    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(m_deadList.Get()));
//
//    return;
//}