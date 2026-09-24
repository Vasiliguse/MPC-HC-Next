#pragma once

#include "stdafx.h"
#include "IAllocatorPresenter.h"
#include <d3d12.h>
#include <dxgi1_6.h>

class CD3D12Renderer final {
public:
    struct OutputInfo {
        bool valid = false;
        bool hdrSupported = false;
        DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        DXGI_OUTPUT_DESC1 desc = {};
    };

    HRESULT Initialize(HWND hWnd, const ExtraRendererSettings& settings);
    HRESULT Resize(UINT width, UINT height);
    HRESULT Present(UINT syncInterval = 1);
    HRESULT PresentTexture(ID3D12Resource* source, D3D12_RESOURCE_STATES sourceState = D3D12_RESOURCE_STATE_COPY_SOURCE);
    HRESULT SetHDR10Metadata(const DXGI_HDR_METADATA_HDR10* metadata);
    HRESULT Reset();

    bool IsInitialized() const { return m_swapChain != nullptr && m_device != nullptr; }
    bool IsDeviceLost() const { return m_deviceLost; }
    const OutputInfo& GetOutputInfo() const { return m_output; }
    ID3D12Device* GetDevice() const { return m_device; }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue; }
    IDXGISwapChain4* GetSwapChain() const { return m_swapChain; }
    DXGI_FORMAT GetSwapChainFormat() const { return m_swapChainFormat; }

private:
    HRESULT CreateDeviceAndSwapChain();
    HRESULT SelectAdapter();
    HRESULT UpdateOutputInfo();
    HRESULT ConfigureSwapChainColorSpace();
    HRESULT CreateFrameResources();
    HRESULT CreateRenderTargetViews();
    HRESULT WaitForGpu();
    HRESULT SignalAndWait();
    bool IsTearingSupported() const;
    bool IsHdrOutputRequested() const;
    void ReleaseDevice();

    HWND m_hWnd = nullptr;
    ExtraRendererSettings m_settings = {};
    CComPtr<IDXGIFactory6> m_factory;
    CComPtr<IDXGIAdapter1> m_adapter;
    CComPtr<ID3D12Device> m_device;
    CComPtr<ID3D12CommandQueue> m_commandQueue;
    CComPtr<IDXGISwapChain4> m_swapChain;
    CComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    std::vector<CComPtr<ID3D12CommandAllocator>> m_commandAllocators;
    CComPtr<ID3D12GraphicsCommandList> m_commandList;
    CComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;
    UINT m_rtvDescriptorSize = 0;
    UINT m_frameIndex = 0;
    CComPtr<IDXGIOutput6> m_output6;
    OutputInfo m_output = {};
    bool m_tearingSupported = false;
    bool m_deviceLost = false;
    DXGI_FORMAT m_swapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
};
