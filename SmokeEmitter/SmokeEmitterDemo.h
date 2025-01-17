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

#pragma once

#include "DXSample.h"
#include "SimpleCamera.h"
#include "StepTimer.h"
#include "ParticleSystem.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <tchar.h>
#include "Scene.h"
#ifdef _DEBUG
#define DX12_ENABLE_DEBUG_LAYER
#endif

#ifdef DX12_ENABLE_DEBUG_LAYER
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include "imgui_internal.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().
using Microsoft::WRL::ComPtr;

//ComPtr<ID3D12Device> GetDevice()
//{
//
//}



class SmokeEmitterDemo : public DXSample
{
public:
    SmokeEmitterDemo(UINT width, UINT height, std::wstring name);

    bool show_demo_window = false;
    bool show_another_window = false;

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnKeyDown(UINT8 key);
    virtual void OnKeyUp(UINT8 key);
    virtual void OnResize(UINT width, UINT height);

private:
    static const UINT FrameCount = 2;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

    struct PointLight
    {
        XMFLOAT3 Postion;
        FLOAT intensity;
        XMFLOAT4 Color;
    };

    struct DirectionLight
    {
        XMFLOAT3 Direction;
        FLOAT intensity;
        XMFLOAT4 Color;
    };
    struct SceneConstantBuffer
    {
        XMFLOAT4X4 ViewProjInv;
        XMFLOAT4X4 View;
        XMFLOAT4X4 ViewProj;
        XMFLOAT4 ViewPos;
        XMFLOAT4 UpDirection;
        PointLight Lightings[2];
        PP particleProps;
        FLOAT elapsedSeconds;
        XMFLOAT3 lookDirection;
        FLOAT randomBuffer[20];

        UINT ScreenWidth;
        UINT ScreenHeight;

        FLOAT innerPadding[2];
        DirectionLight DirLights[2];
        FLOAT padding[44];

    };
    static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
    
    // Pipeline objects.
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12PipelineState> m_opaquePSO;
 

    //int m_aliveSlots[2] = { 3, 4 };
    float m_clearColor[4] = { 116.0f / 255.0f, 157.0f / 255.0f, 198.0f / 255.0f, 1.0f };

    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    ComPtr<ID3D12DescriptorHeap> m_cbvSrvUavHeap;
    UINT m_cbvSrvUavDescriptorSize;

    // Scene object settings.
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

    std::vector<Vertex> m_sceneVertices;

    // Particle system.
    Emitter m_emitter;

    // vertex buffer
    ComPtr<ID3D12Resource> m_sharedVertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_sharedVertexBufferView;

    //ComPtr<ID3D12Resource> m_instanceBuffer;
    //D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
    //UINT8* m_pInstanceDataBegin;

    //ComPtr<ID3D12Resource> m_indexBuffer;
    //D3D12_INDEX_  BUFFER_VIEW m_indexBufferView;

    ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    // Camera settings.
    SimpleCamera m_camera;
    StepTimer m_timer;

    // Synchronization objects.
    UINT m_frameIndex;
    UINT m_frameCounter;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[FrameCount];

    void LoadPipeline();
    void LoadAssets();

    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();

    void LoadScenes();

    void sceneObjectDraw();
};
