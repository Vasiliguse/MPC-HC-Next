/*
 * D3D11 presentation foundation.
 *
 * The class deliberately does not become the default renderer yet. It is a
 * production-safe lifecycle primitive for the upcoming modern presentation
 * backend while D3D9/EVR remain the compatibility path.
 */

#include "stdafx.h"
#include "D3D11Renderer.h"

#include <VersionHelpers.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
constexpr UINT kSwapChainBufferCount = 3;

bool IsHdr10ColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace)
{
    return colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        || colorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020;
}
}

CD3D11Renderer::CD3D11Renderer() = default;

CD3D11Renderer::~CD3D11Renderer()
{
    ReleaseDevice();
}

HRESULT CD3D11Renderer::Initialize(HWND hWnd, const ExtraRendererSettings& settings)
{
    if (!hWnd || !::IsWindow(hWnd)) {
        return E_INVALIDARG;
    }

    ReleaseDevice();

    m_hWnd = hWnd;
    m_settings = settings;

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

    hr = ConfigureSwapChainColorSpace();
    if (FAILED(hr)) {
        ReleaseDevice();
        return hr;
    }

    return S_OK;
}

HRESULT CD3D11Renderer::SelectAdapter()
{
    CComPtr<IDXGIOutput> targetOutput;
    HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);

    for (UINT adapterIndex = 0;; ++adapterIndex) {
        CComPtr<IDXGIAdapter1> adapter;
        HRESULT hr = m_factory->EnumAdapterByGpuPreference(
            adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(&adapter));

        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc = {};
        if (FAILED(adapter->GetDesc1(&desc)) || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            continue;
        }

        for (UINT outputIndex = 0;; ++outputIndex) {
            CComPtr<IDXGIOutput> output;
            if (adapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (!output) {
                continue;
            }

            DXGI_OUTPUT_DESC outputDesc = {};
            if (FAILED(output->GetDesc(&outputDesc))) {
                continue;
            }

            if (outputDesc.Monitor == monitor) {
                m_adapter = adapter;
                return S_OK;
            }
        }

        if (!m_adapter) {
            m_adapter = adapter;
        }
    }

    return m_adapter ? S_OK : DXGI_ERROR_NOT_FOUND;
}

HRESULT CD3D11Renderer::CreateDeviceAndSwapChain()
{
    static constexpr D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
#if defined(_DEBUG)
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        m_adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        flags,
        levels,
        _countof(levels),
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context);

#if defined(_DEBUG)
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING) {
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            m_adapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            nullptr,
            flags,
            levels,
            _countof(levels),
            D3D11_SDK_VERSION,
            &m_device,
            &featureLevel,
            &m_context);
    }
#endif

    if (FAILED(hr)) {
        return hr;
    }

    CComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (FAILED(hr)) {
        return hr;
    }

    m_allowTearing = IsTearingSupported();

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 0;
    desc.Height = 0;
    m_swapChainFormat = GetSwapChainFormat();
    desc.Format = m_swapChainFormat;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    desc.BufferCount = kSwapChainBufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    hr = m_factory->CreateSwapChainForHwnd(
        m_device,
        m_hWnd,
        &desc,
        nullptr,
        nullptr,
        &m_swapChain);
    if (FAILED(hr)) {
        return hr;
    }

    // The swap chain owns presentation state; do not allow DXGI to inject
    // legacy Alt+Enter handling into the application's window manager.
    m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

    return S_OK;
}

bool CD3D11Renderer::IsTearingSupported() const
{
    BOOL supported = FALSE;
    return SUCCEEDED(m_factory->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported)))
        && supported;
}

HRESULT CD3D11Renderer::UpdateOutputInfo()
{
    m_output = {};

    if (!m_swapChain) {
        return E_UNEXPECTED;
    }

    CComPtr<IDXGIOutput> output;
    HRESULT hr = m_swapChain->GetContainingOutput(&output);
    if (FAILED(hr)) {
        return hr;
    }

    hr = output->QueryInterface(IID_PPV_ARGS(&m_outputObject));
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_outputObject->GetDesc1(&m_output.desc);
    if (FAILED(hr)) {
        return hr;
    }

    m_output.valid = true;
    m_output.colorSpace = m_output.desc.ColorSpace;
    // BitsPerColor alone is not sufficient: many SDR outputs expose a
    // 10-bit panel. HDR10 capability is represented by the active DXGI
    // output color space, while scRGB is handled separately in a later
    // presentation path.
    m_output.hdrSupported = IsHdr10ColorSpace(m_output.desc.ColorSpace);

    return S_OK;
}

HRESULT CD3D11Renderer::ConfigureSwapChainColorSpace()
{
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }

    // Keep SDR as the safe default. HDR10 is selected only when explicitly
    // requested and the active output advertises an HDR-capable color space.
    DXGI_COLOR_SPACE_TYPE colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

    if (m_settings.bEnableHDR
        && m_settings.iOutputColorMode == VIDEO_OUTPUT_COLOR_HDR10
        && m_output.hdrSupported) {
        colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
    }

    CComPtr<IDXGISwapChain3> swapChain3;
    HRESULT hr = m_swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3));
    if (FAILED(hr)) {
        return hr;
    }

    return swapChain3->SetColorSpace1(colorSpace);
}

bool CD3D11Renderer::IsHdrOutputRequested() const
{
    return m_settings.bEnableHDR
        && m_settings.iOutputColorMode == VIDEO_OUTPUT_COLOR_HDR10
        && m_output.hdrSupported;
}

DXGI_FORMAT CD3D11Renderer::GetSwapChainFormat() const
{
    return IsHdrOutputRequested()
        ? DXGI_FORMAT_R10G10B10A2_UNORM
        : DXGI_FORMAT_B8G8R8A8_UNORM;
}

HRESULT CD3D11Renderer::Resize(UINT width, UINT height)
{
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }

    if (!width || !height) {
        return S_FALSE;
    }

    m_context->OMSetRenderTargets(0, nullptr, nullptr);

    HRESULT hr = m_swapChain->ResizeBuffers(
        kSwapChainBufferCount,
        width,
        height,
        m_swapChainFormat,
        m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        m_deviceLost = true;
    }

    if (SUCCEEDED(hr)) {
        hr = UpdateOutputInfo();
        if (SUCCEEDED(hr)) {
            hr = ConfigureSwapChainColorSpace();
        }
    }

    return hr;
}

HRESULT CD3D11Renderer::Present(UINT syncInterval, UINT presentFlags)
{
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }

    if (m_allowTearing && syncInterval == 0) {
        presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
    }

    const HRESULT hr = m_swapChain->Present(syncInterval, presentFlags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        m_deviceLost = true;
    }

    return hr;
}

HRESULT CD3D11Renderer::Reset()
{
    if (!m_hWnd || !::IsWindow(m_hWnd)) {
        return E_INVALIDARG;
    }

    const auto settings = m_settings;
    const HWND hWnd = m_hWnd;
    return Initialize(hWnd, settings);
}

void CD3D11Renderer::ReleaseDevice()
{
    m_outputObject.Release();
    m_swapChain.Release();
    m_context.Release();
    m_device.Release();
    m_adapter.Release();
    m_factory.Release();

    m_output = {};
    m_allowTearing = false;
    m_deviceLost = false;
    m_swapChainFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
}
