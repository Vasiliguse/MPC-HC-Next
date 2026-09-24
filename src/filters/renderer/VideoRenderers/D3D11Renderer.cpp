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

    const DXGI_FORMAT desiredFormat = GetSwapChainFormat();
    if (desiredFormat != m_swapChainFormat) {
        RECT clientRect = {};
        ::GetClientRect(m_hWnd, &clientRect);
        const UINT width = std::max<LONG>(1, clientRect.right - clientRect.left);
        const UINT height = std::max<LONG>(1, clientRect.bottom - clientRect.top);
        hr = m_swapChain->ResizeBuffers(
            kSwapChainBufferCount, width, height, desiredFormat,
            m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
        if (FAILED(hr)) {
            ReleaseDevice();
            return hr;
        }
        m_swapChainFormat = desiredFormat;
    }

    hr = CreateBackBufferViews();
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

    hr = m_device->QueryInterface(IID_PPV_ARGS(&m_videoDevice));
    if (FAILED(hr)) return hr;
    hr = m_context->QueryInterface(IID_PPV_ARGS(&m_videoContext));
    if (FAILED(hr)) return hr;

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

    UINT support = 0;
    hr = swapChain3->CheckColorSpaceSupport(colorSpace, &support);
    if (FAILED(hr) || !(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
        return FAILED(hr) ? hr : DXGI_ERROR_UNSUPPORTED;
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

HRESULT CD3D11Renderer::SetHDR10Metadata(const DXGI_HDR_METADATA_HDR10* metadata)
{
    if (!m_swapChain) {
        return E_UNEXPECTED;
    }
    if (!IsHdrOutputRequested()) {
        return DXGI_ERROR_UNSUPPORTED;
    }

    CComPtr<IDXGISwapChain4> swapChain4;
    HRESULT hr = m_swapChain->QueryInterface(IID_PPV_ARGS(&swapChain4));
    if (FAILED(hr)) {
        return hr;
    }
    if (!metadata) {
        return swapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr);
    }
    return swapChain4->SetHDRMetaData(
        DXGI_HDR_METADATA_TYPE_HDR10, sizeof(DXGI_HDR_METADATA_HDR10), const_cast<DXGI_HDR_METADATA_HDR10*>(metadata));
}

HRESULT CD3D11Renderer::CreateBackBufferViews()
{
    if (!m_swapChain || !m_device) return E_UNEXPECTED;
    m_backBufferRTV.Release();

    CComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_backBufferRTV);
    if (FAILED(hr)) return hr;
    return S_OK;
}

void CD3D11Renderer::ReleaseFrameResources()
{
    m_videoProcessor.Release();
    m_videoProcessorEnumerator.Release();
    m_backBufferRTV.Release();
    m_videoWidth = 0;
    m_videoHeight = 0;
}

HRESULT CD3D11Renderer::EnsureVideoProcessor(D3D11_VIDEO_FRAME_FORMAT format, UINT width, UINT height)
{
    if (!m_videoDevice) return E_UNEXPECTED;
    if (m_videoProcessor && m_videoProcessorEnumerator && m_videoWidth == width && m_videoHeight == height) return S_OK;

    m_videoProcessor.Release();
    m_videoProcessorEnumerator.Release();

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
    desc.InputFrameFormat = format;
    desc.InputWidth = width;
    desc.InputHeight = height;
    desc.OutputWidth = width;
    desc.OutputHeight = height;
    desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_videoDevice->CreateVideoProcessorEnumerator(&desc, &m_videoProcessorEnumerator);
    if (FAILED(hr)) return hr;

    UINT support = 0;
    hr = m_videoProcessorEnumerator->CheckVideoProcessorFormat(m_swapChainFormat, &support);
    if (FAILED(hr) || !(support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) return FAILED(hr) ? hr : DXGI_ERROR_UNSUPPORTED;

    hr = m_videoDevice->CreateVideoProcessor(m_videoProcessorEnumerator, 0, &m_videoProcessor);
    if (FAILED(hr)) return hr;

    m_videoWidth = width;
    m_videoHeight = height;
    return S_OK;
}

bool CD3D11Renderer::IsAdapterCompatible(ID3D11Device* device) const
{
    if (!device || !m_adapter) return false;
    CComPtr<IDXGIDevice> dxgiDevice;
    CComPtr<IDXGIAdapter> adapter;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) || FAILED(dxgiDevice->GetAdapter(&adapter))) return false;
    DXGI_ADAPTER_DESC desc = {};
    DXGI_ADAPTER_DESC selected = {};
    if (FAILED(adapter->GetDesc(&desc)) || FAILED(m_adapter->GetDesc(&selected))) return false;
    return desc.AdapterLuid.HighPart == selected.AdapterLuid.HighPart && desc.AdapterLuid.LowPart == selected.AdapterLuid.LowPart;
}

HRESULT CD3D11Renderer::PresentMediaSample(IMediaSample* sample)
{
    if (!sample) {
        return E_POINTER;
    }

    CComQIPtr<IMediaSampleD3D11> d3d11Sample(sample);
    if (!d3d11Sample) {
        return E_NOINTERFACE;
    }

    CComPtr<ID3D11Texture2D> texture;
    UINT arraySlice = 0;
    HRESULT hr = d3d11Sample->GetD3D11Texture(0, &texture, &arraySlice);
    if (FAILED(hr)) {
        return hr;
    }

    return PresentD3D11Texture(texture, arraySlice);
}

HRESULT CD3D11Renderer::PresentD3D11Texture(ID3D11Texture2D* texture, UINT arraySlice)
{
    if (!texture || !m_swapChain || !m_context || !m_videoDevice || !m_videoContext) return E_INVALIDARG;
    CComPtr<ID3D11Device> textureDevice;
    CComPtr<IDXGIDevice> textureDxgiDevice;
    CComPtr<IDXGIAdapter> textureAdapter;
    texture->GetDevice(&textureDevice);
    if (!textureDevice ||
        FAILED(textureDevice->QueryInterface(IID_PPV_ARGS(&textureDxgiDevice))) ||
        FAILED(textureDxgiDevice->GetAdapter(&textureAdapter))) {
        return E_INVALIDARG;
    }
    DXGI_ADAPTER_DESC textureAdapterDesc = {};
    DXGI_ADAPTER_DESC rendererAdapterDesc = {};
    if (FAILED(textureAdapter->GetDesc(&textureAdapterDesc)) || FAILED(m_adapter->GetDesc(&rendererAdapterDesc)) ||
        textureAdapterDesc.AdapterLuid.HighPart != rendererAdapterDesc.AdapterLuid.HighPart ||
        textureAdapterDesc.AdapterLuid.LowPart != rendererAdapterDesc.AdapterLuid.LowPart) {
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    texture->GetDesc(&textureDesc);
    if (!textureDesc.Width || !textureDesc.Height) return E_INVALIDARG;

    D3D11_VIDEO_FRAME_FORMAT frameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    HRESULT hr = EnsureVideoProcessor(frameFormat, textureDesc.Width, textureDesc.Height);
    if (FAILED(hr)) return hr;

    UINT inputSupport = 0;
    hr = m_videoProcessorEnumerator->CheckVideoProcessorFormat(textureDesc.Format, &inputSupport);
    if (FAILED(hr) || !(inputSupport & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT)) return FAILED(hr) ? hr : DXGI_ERROR_UNSUPPORTED;
    if (FAILED(hr)) return hr;

    CComPtr<ID3D11VideoProcessorInputView> inputView;
    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputDesc = {};
    inputDesc.FourCC = 0;
    inputDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    inputDesc.Texture2D.MipSlice = 0;
    inputDesc.Texture2D.ArraySlice = arraySlice;
    hr = m_videoDevice->CreateVideoProcessorInputView(texture, m_videoProcessorEnumerator, &inputDesc, &inputView);
    if (FAILED(hr)) return hr;

    CComPtr<ID3D11Texture2D> backBuffer;
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    CComPtr<ID3D11VideoProcessorOutputView> outputView;
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputDesc = {};
    outputDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    outputDesc.Texture2D.MipSlice = 0;
    hr = m_videoDevice->CreateVideoProcessorOutputView(backBuffer, m_videoProcessorEnumerator, &outputDesc, &outputView);
    if (FAILED(hr)) return hr;

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = inputView;

    D3D11_TEXTURE2D_DESC outputDesc2D = {};
    backBuffer->GetDesc(&outputDesc2D);

    RECT sourceRect = { 0, 0, static_cast<LONG>(textureDesc.Width), static_cast<LONG>(textureDesc.Height) };
    RECT destRect = { 0, 0, static_cast<LONG>(outputDesc2D.Width), static_cast<LONG>(outputDesc2D.Height) };

    m_videoContext->VideoProcessorSetStreamSourceRect(
        m_videoProcessor, 0, TRUE, &sourceRect);

    m_videoContext->VideoProcessorSetOutputTargetRect(
        m_videoProcessor, TRUE, &destRect);

    return m_videoContext->VideoProcessorBlt(m_videoProcessor, outputView, 0, 1, &stream);
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
    ReleaseFrameResources();

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
            const DXGI_FORMAT desiredFormat = GetSwapChainFormat();
            if (desiredFormat != m_swapChainFormat) {
                hr = m_swapChain->ResizeBuffers(
                    kSwapChainBufferCount, width, height, desiredFormat,
                    m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
                if (SUCCEEDED(hr)) {
                    m_swapChainFormat = desiredFormat;
                }
            }
            if (SUCCEEDED(hr)) {
                hr = CreateBackBufferViews();
            }
            if (SUCCEEDED(hr)) {
                hr = ConfigureSwapChainColorSpace();
            }
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
    ReleaseFrameResources();
    m_videoContext.Release();
    m_videoDevice.Release();
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
