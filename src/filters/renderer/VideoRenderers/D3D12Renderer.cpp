#include "stdafx.h"
#include "D3D12Renderer.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
constexpr UINT kBufferCount = 3;

bool IsHdr10ColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace) {
    return colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 ||
           colorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020;
}

bool IsDeviceLostHr(HRESULT hr) {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
           hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}
}

HRESULT CD3D12Renderer::Initialize(HWND hWnd, const ExtraRendererSettings& settings) {
    if (!hWnd || !IsWindow(hWnd)) {
        return E_INVALIDARG;
    }

    m_hWnd = hWnd;
    m_settings = settings;
    m_deviceLost = false;
    m_output = {};
    m_swapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

    ReleaseDevice();

    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_factory));
    if (FAILED(hr)) {
        return hr;
    }

    hr = SelectAdapter();
    if (FAILED(hr)) {
        ReleaseDevice();
        return hr;
    }

    hr = CreateDeviceAndSwapChain();
    if (FAILED(hr)) {
        ReleaseDevice();
        return hr;
    }

    hr = UpdateOutputInfo();
    if (FAILED(hr)) {
        ReleaseDevice();
        return hr;
    }

    const DXGI_FORMAT desiredFormat = IsHdrOutputRequested()
        ? DXGI_FORMAT_R10G10B10A2_UNORM
        : DXGI_FORMAT_B8G8R8A8_UNORM;

    if (desiredFormat != m_swapChainFormat) {
        m_swapChain->ResizeBuffers(kBufferCount, 0, 0, desiredFormat,
            DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
            (m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
        m_swapChainFormat = desiredFormat;
    }

    return ConfigureSwapChainColorSpace();
}

HRESULT CD3D12Renderer::SelectAdapter() {
    CComPtr<IDXGIOutput> windowOutput;
    HRESULT hr = m_factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&m_adapter));
    if (FAILED(hr)) {
        return hr;
    }

    HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    for (UINT i = 0; ; ++i) {
        CComPtr<IDXGIAdapter1> adapter;
        if (m_factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter)) == DXGI_ERROR_NOT_FOUND) {
            break;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            continue;
        }

        for (UINT j = 0; ; ++j) {
            CComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(j, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_OUTPUT_DESC outputDesc = {};
            if (SUCCEEDED(output->GetDesc(&outputDesc)) && outputDesc.Monitor == monitor) {
                m_adapter = adapter;
                return S_OK;
            }
        }
    }

    return m_adapter ? S_OK : DXGI_ERROR_NOT_FOUND;
}

HRESULT CD3D12Renderer::CreateDeviceAndSwapChain() {
    HRESULT hr = D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
    if (FAILED(hr)) {
        hr = D3D12CreateDevice(m_adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    }
    if (FAILED(hr)) {
        return hr;
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr)) {
        return hr;
    }

    RECT rc = {};
    GetClientRect(m_hWnd, &rc);
    const UINT width = std::max<UINT>(1, rc.right - rc.left);
    const UINT height = std::max<UINT>(1, rc.bottom - rc.top);

    m_tearingSupported = IsTearingSupported();
    const UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
        (m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferCount = kBufferCount;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.SampleDesc.Count = 1;
    desc.Flags = flags;

    CComPtr<IDXGISwapChain1> swapChain;
    hr = m_factory->CreateSwapChainForHwnd(m_commandQueue, m_hWnd, &desc, nullptr, nullptr, &swapChain);
    if (FAILED(hr)) {
        return hr;
    }

    m_swapChain = swapChain;
    m_swapChainFormat = desc.Format;
    return m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
}

HRESULT CD3D12Renderer::UpdateOutputInfo() {
    m_output = {};
    m_output6.Release();

    CComPtr<IDXGIOutput> output;
    HRESULT hr = m_swapChain->GetContainingOutput(&output);
    if (FAILED(hr)) {
        return hr;
    }

    hr = output->QueryInterface(IID_PPV_ARGS(&m_output6));
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_output6->GetDesc1(&m_output.desc);
    if (SUCCEEDED(hr)) {
        m_output.valid = true;
        m_output.colorSpace = m_output.desc.ColorSpace;
        m_output.hdrSupported = IsHdr10ColorSpace(m_output.colorSpace);
    }
    return hr;
}

HRESULT CD3D12Renderer::ConfigureSwapChainColorSpace() {
    CComQIPtr<IDXGISwapChain3> swapChain3 = m_swapChain;
    if (!swapChain3) {
        return E_NOINTERFACE;
    }

    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
    if (IsHdrOutputRequested()) {
        colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    }

    UINT support = 0;
    HRESULT hr = swapChain3->CheckColorSpaceSupport(colorSpace, &support);
    if (FAILED(hr) || !(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
        return DXGI_ERROR_UNSUPPORTED;
    }

    return swapChain3->SetColorSpace1(colorSpace);
}

bool CD3D12Renderer::IsTearingSupported() const {
    CComPtr<IDXGIFactory5> factory5;
    if (FAILED(m_factory->QueryInterface(IID_PPV_ARGS(&factory5)))) {
        return false;
    }

    BOOL allowTearing = FALSE;
    return SUCCEEDED(factory5->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))) && allowTearing;
}

bool CD3D12Renderer::IsHdrOutputRequested() const {
    return m_settings.bEnableHDR &&
        m_settings.iOutputColorMode == VIDEO_OUTPUT_COLOR_HDR10 &&
        m_output.valid && m_output.hdrSupported;
}

HRESULT CD3D12Renderer::SetHDR10Metadata(const DXGI_HDR_METADATA_HDR10* metadata) {
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }
    if (!IsHdrOutputRequested()) {
        return DXGI_ERROR_UNSUPPORTED;
    }
    if (!metadata) {
        return m_swapChain->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
    }
    return m_swapChain->SetHDRMetaData(
        DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), const_cast<DXGI_HDR_METADATA_HDR10*>(metadata));
}

HRESULT CD3D12Renderer::Resize(UINT width, UINT height) {
    if (!m_swapChain || width == 0 || height == 0) {
        return E_INVALIDARG;
    }

    const UINT flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH |
        (m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

    HRESULT hr = m_swapChain->ResizeBuffers(kBufferCount, width, height, m_swapChainFormat, flags);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    hr = UpdateOutputInfo();
    if (FAILED(hr)) {
        return hr;
    }

    const DXGI_FORMAT desiredFormat = IsHdrOutputRequested()
        ? DXGI_FORMAT_R10G10B10A2_UNORM
        : DXGI_FORMAT_B8G8R8A8_UNORM;
    if (desiredFormat != m_swapChainFormat) {
        hr = m_swapChain->ResizeBuffers(kBufferCount, width, height, desiredFormat, flags);
        if (FAILED(hr)) {
            if (IsDeviceLostHr(hr)) {
                m_deviceLost = true;
            }
            return hr;
        }
        m_swapChainFormat = desiredFormat;
    }

    return ConfigureSwapChainColorSpace();
}

HRESULT CD3D12Renderer::Present(UINT syncInterval) {
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }

    UINT flags = 0;
    if (m_tearingSupported && syncInterval == 0) {
        flags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    const HRESULT hr = m_swapChain->Present(syncInterval, flags);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    return hr;
}

HRESULT CD3D12Renderer::Reset() {
    if (!m_hWnd) {
        return E_UNEXPECTED;
    }
    return Initialize(m_hWnd, m_settings);
}

void CD3D12Renderer::ReleaseDevice() {
    m_swapChain.Release();
    m_commandQueue.Release();
    m_device.Release();
    m_output6.Release();
    m_adapter.Release();
    m_factory.Release();
    m_output = {};
    m_deviceLost = false;
}
