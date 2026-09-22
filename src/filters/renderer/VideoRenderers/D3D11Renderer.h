/*
 * D3D11 presentation foundation.
 *
 * This layer is intentionally independent from the legacy D3D9/EVR presenters.
 * It provides device/swap-chain lifecycle and output capability detection so
 * the modern renderer can be integrated without changing the legacy fallback.
 */

#pragma once

#include "stdafx.h"
#include "IAllocatorPresenter.h"

#include <d3d11.h>
#include <dxgi1_6.h>

class CD3D11Renderer
{
public:
    struct OutputInfo {
        bool valid = false;
        bool hdrSupported = false;
        DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        DXGI_OUTPUT_DESC1 desc = {};
    };

    CD3D11Renderer();
    ~CD3D11Renderer();

    CD3D11Renderer(const CD3D11Renderer&) = delete;
    CD3D11Renderer& operator=(const CD3D11Renderer&) = delete;

    HRESULT Initialize(HWND hWnd, const ExtraRendererSettings& settings);
    HRESULT Resize(UINT width, UINT height);
    HRESULT Present(UINT syncInterval, UINT presentFlags);
    HRESULT Reset();

    bool IsInitialized() const { return m_swapChain != nullptr; }
    bool IsDeviceLost() const { return m_deviceLost; }
    const OutputInfo& GetOutputInfo() const { return m_output; }

    ID3D11Device* GetDevice() const { return m_device; }
    ID3D11DeviceContext* GetContext() const { return m_context; }
    IDXGISwapChain1* GetSwapChain() const { return m_swapChain; }

private:
    HRESULT CreateDeviceAndSwapChain();
    HRESULT SelectAdapter();
    HRESULT UpdateOutputInfo();
    HRESULT ConfigureSwapChainColorSpace();
    bool IsTearingSupported() const;
    bool IsHdrOutputRequested() const;
    DXGI_FORMAT GetSwapChainFormat() const;
    void ReleaseDevice();

    HWND m_hWnd = nullptr;
    ExtraRendererSettings m_settings = {};

    CComPtr<IDXGIFactory6> m_factory;
    CComPtr<IDXGIAdapter1> m_adapter;
    CComPtr<ID3D11Device> m_device;
    CComPtr<ID3D11DeviceContext> m_context;
    CComPtr<IDXGISwapChain1> m_swapChain;
    CComPtr<IDXGIOutput6> m_outputObject;

    OutputInfo m_output = {};
    bool m_allowTearing = false;
    bool m_deviceLost = false;
    DXGI_FORMAT m_swapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
};
