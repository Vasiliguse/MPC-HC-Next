/*
 * D3D11 presentation foundation.
 *
 * The class deliberately does not become the default renderer yet. It is a
 * production-safe lifecycle primitive for the upcoming modern presentation
 * backend while D3D9/EVR remain the compatibility path.
 */

#include "stdafx.h"
#include "D3D11Renderer.h"
#include <algorithm>
#include <cmath>
#include <IMediaSideData.h>

#include <VersionHelpers.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
constexpr UINT kSwapChainBufferCount = 3;

class ScopedDecoderMutex
{
public:
    explicit ScopedDecoderMutex(HANDLE mutex) : m_mutex(mutex)
    {
        if (m_mutex) {
            const DWORD wait = WaitForSingleObject(m_mutex, INFINITE);
            m_locked = (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED);
        } else {
            m_locked = true;
        }
    }

    ~ScopedDecoderMutex()
    {
        if (m_mutex && m_locked) {
            ReleaseMutex(m_mutex);
        }
    }

    bool Locked() const { return m_locked; }

private:
    HANDLE m_mutex = nullptr;
    bool m_locked = false;
};

bool IsHdr10ColorSpace(DXGI_COLOR_SPACE_TYPE colorSpace)
{
    return colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
        || colorSpace == DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020;
}
}

CD3D11Renderer::CD3D11Renderer() = default;

void CD3D11Renderer::SetInputColorInfo(UINT transferMatrix, UINT nominalRange, UINT sourceHeight)
{
    // DXVA2 values: 0 = unknown, 1 = BT.709, 2 = BT.601, 3 = SMPTE 240M.
    // D3D11's legacy color-space state exposes BT.601/BT.709 only, so unknown
    // follows Microsoft's SD/HD default and SMPTE 240M is mapped to BT.709.
    if (transferMatrix == 0) {
        transferMatrix = sourceHeight > 576 ? 1u : 2u;
    }

    m_inputColorSpace = {};
    m_inputColorSpace.Usage = 0;
    m_inputColorSpace.RGB_Range = 0;
    m_inputColorSpace.YCbCr_Matrix = (transferMatrix == 2u) ? 0u : 1u;
    m_inputColorSpace.YCbCr_xvYCC = 0;
    m_inputColorSpace.Nominal_Range =
        (nominalRange == 2u || nominalRange == 3u)
        ? D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235
        : D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
}

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
    DrainPendingFrames(true);
    m_pendingFrames.clear();

    m_videoProcessor.Release();
    m_videoProcessorEnumerator.Release();
    m_backBufferRTV.Release();
    m_videoWidth = 0;
    m_videoHeight = 0;
    m_processorOutputWidth = 0;
    m_processorOutputHeight = 0;
}

HRESULT CD3D11Renderer::EnsureVideoProcessor(D3D11_VIDEO_FRAME_FORMAT format, UINT inputWidth, UINT inputHeight, UINT outputWidth, UINT outputHeight)
{
    if (!m_videoDevice) return E_UNEXPECTED;
    if (m_videoProcessor && m_videoProcessorEnumerator
        && m_videoWidth == inputWidth && m_videoHeight == inputHeight
        && m_processorOutputWidth == outputWidth && m_processorOutputHeight == outputHeight) {
        return S_OK;
    }

    m_videoProcessor.Release();
    m_videoProcessorEnumerator.Release();

    D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
    desc.InputFrameFormat = format;
    desc.InputWidth = inputWidth;
    desc.InputHeight = inputHeight;
    desc.OutputWidth = outputWidth;
    desc.OutputHeight = outputHeight;
    desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

    HRESULT hr = m_videoDevice->CreateVideoProcessorEnumerator(&desc, &m_videoProcessorEnumerator);
    if (FAILED(hr)) return hr;

    UINT support = 0;
    hr = m_videoProcessorEnumerator->CheckVideoProcessorFormat(m_swapChainFormat, &support);
    if (FAILED(hr) || !(support & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT)) return FAILED(hr) ? hr : DXGI_ERROR_UNSUPPORTED;

    hr = m_videoDevice->CreateVideoProcessor(m_videoProcessorEnumerator, 0, &m_videoProcessor);
    if (FAILED(hr)) return hr;

    m_videoWidth = inputWidth;
    m_videoHeight = inputHeight;
    m_processorOutputWidth = outputWidth;
    m_processorOutputHeight = outputHeight;
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

HRESULT CD3D11Renderer::DrainPendingFrames(bool waitForAll)
{
    if (!m_context) {
        m_pendingFrames.clear();
        return S_OK;
    }

    if (waitForAll && !m_pendingFrames.empty()) {
        m_context->Flush();
    }

    while (!m_pendingFrames.empty()) {
        PendingFrame& frame = m_pendingFrames.front();
        BOOL complete = FALSE;
        HRESULT hr = m_context->GetData(
            frame.query, &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (FAILED(hr)) {
            return hr;
        }

        if (!complete) {
            if (!waitForAll) {
                break;
            }
            Sleep(1);
            continue;
        }

        m_pendingFrames.pop_front();
    }

    return S_OK;
}

HRESULT CD3D11Renderer::SetHDR10MetadataFromSample(IMediaSample* sample)
{
    if (!sample) {
        return E_POINTER;
    }

    CComQIPtr<IMediaSideData> sideData(sample);
    if (!sideData) {
        return SetHDR10Metadata(nullptr);
    }

    const BYTE* data = nullptr;
    size_t size = 0;
    DXGI_HDR_METADATA_HDR10 dxgi = {};

    bool hasMastering = false;
    if (SUCCEEDED(sideData->GetSideData(IID_MediaSideDataHDR, &data, &size)) &&
        data && size >= sizeof(MediaSideDataHDR)) {
        const auto* hdr = reinterpret_cast<const MediaSideDataHDR*>(data);

        const auto toChromaticity = [](double value) -> UINT16 {
            if (!std::isfinite(value)) {
                return 0;
            }
            const double scaled = std::clamp(value * 50000.0, 0.0, 50000.0);
            return static_cast<UINT16>(std::llround(scaled));
        };

        // MediaSideDataHDR is stored in G-B-R order, while DXGI HDR10
        // metadata expects R-G-B primary order.
        dxgi.RedPrimary[0] = toChromaticity(hdr->display_primaries_x[2]);
        dxgi.RedPrimary[1] = toChromaticity(hdr->display_primaries_y[2]);
        dxgi.GreenPrimary[0] = toChromaticity(hdr->display_primaries_x[0]);
        dxgi.GreenPrimary[1] = toChromaticity(hdr->display_primaries_y[0]);
        dxgi.BluePrimary[0] = toChromaticity(hdr->display_primaries_x[1]);
        dxgi.BluePrimary[1] = toChromaticity(hdr->display_primaries_y[1]);

        dxgi.WhitePoint[0] = toChromaticity(hdr->white_point_x);
        dxgi.WhitePoint[1] = toChromaticity(hdr->white_point_y);

        const auto toLuminance = [](double value) -> UINT {
            if (!std::isfinite(value) || value <= 0.0) {
                return 0;
            }
            const double scaled = std::clamp(value * 10000.0, 0.0, 4294967295.0);
            return static_cast<UINT>(std::llround(scaled));
        };

        dxgi.MaxMasteringLuminance = toLuminance(hdr->max_display_mastering_luminance);
        dxgi.MinMasteringLuminance = toLuminance(hdr->min_display_mastering_luminance);
        hasMastering = true;
    }

    if (SUCCEEDED(sideData->GetSideData(IID_MediaSideDataHDRContentLightLevel, &data, &size)) &&
        data && size >= sizeof(MediaSideDataHDRContentLightLevel)) {
        const auto* cll = reinterpret_cast<const MediaSideDataHDRContentLightLevel*>(data);
        dxgi.MaxContentLightLevel = static_cast<UINT16>(std::min(cll->MaxCLL, 65535u));
        dxgi.MaxFrameAverageLightLevel = static_cast<UINT16>(std::min(cll->MaxFALL, 65535u));
        hasMastering = true;
    }

    return hasMastering ? SetHDR10Metadata(&dxgi) : SetHDR10Metadata(nullptr);
}

HRESULT CD3D11Renderer::ActivateD3D11Decoding(ID3D11Device* device, ID3D11DeviceContext* context, HANDLE mutex, UINT flags)
{
	UNREFERENCED_PARAMETER(flags);

	if (!device || !context || !m_factory || !m_adapter) {
		return E_INVALIDARG;
	}

	CComPtr<ID3D11Device> contextDevice;
	context->GetDevice(&contextDevice);
	if (contextDevice != device) {
		return E_INVALIDARG;
	}

	if (!IsAdapterCompatible(device)) {
		return DXGI_ERROR_DEVICE_REMOVED;
	}

	RECT clientRect = {};
	::GetClientRect(m_hWnd, &clientRect);
	const UINT width = std::max<LONG>(1, clientRect.right - clientRect.left);
	const UINT height = std::max<LONG>(1, clientRect.bottom - clientRect.top);

	// The decoder owns the D3D11 device that backs native video textures.
	// The swap chain and video processor must use that same device; matching
	// adapter LUIDs alone is not sufficient for cross-device resource access.
	ReleaseFrameResources();
	m_backBufferRTV.Release();
	m_swapChain.Release();
	m_videoContext.Release();
	m_videoDevice.Release();
	m_context.Release();
	m_device.Release();

	m_device = device;
	m_context = context;
	m_decoderMutex = mutex;

	HRESULT hr = m_device->QueryInterface(IID_PPV_ARGS(&m_videoDevice));
	if (FAILED(hr)) {
		return hr;
	}
	hr = m_context->QueryInterface(IID_PPV_ARGS(&m_videoContext));
	if (FAILED(hr)) {
		return hr;
	}

	m_allowTearing = IsTearingSupported();
	m_swapChainFormat = GetSwapChainFormat();

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = width;
	desc.Height = height;
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

	m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

	hr = UpdateOutputInfo();
	if (FAILED(hr)) {
		return hr;
	}

	const DXGI_FORMAT desiredFormat = GetSwapChainFormat();
	if (desiredFormat != m_swapChainFormat) {
		hr = m_swapChain->ResizeBuffers(
			kSwapChainBufferCount, width, height, desiredFormat,
			m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
		if (FAILED(hr)) {
			return hr;
		}
		m_swapChainFormat = desiredFormat;
	}

	hr = CreateBackBufferViews();
	if (FAILED(hr)) {
		return hr;
	}

	return ConfigureSwapChainColorSpace();
}

UINT CD3D11Renderer::GetD3D11AdapterIndex() const
{
	if (!m_factory || !m_adapter) {
		return UINT_MAX;
	}

	DXGI_ADAPTER_DESC1 selected = {};
	if (FAILED(m_adapter->GetDesc1(&selected))) {
		return UINT_MAX;
	}

	for (UINT index = 0; ; ++index) {
		CComPtr<IDXGIAdapter1> adapter;
		HRESULT hr = m_factory->EnumAdapters1(index, &adapter);
		if (hr == DXGI_ERROR_NOT_FOUND) {
			break;
		}
		if (FAILED(hr) || !adapter) {
			continue;
		}

		DXGI_ADAPTER_DESC1 desc = {};
		if (SUCCEEDED(adapter->GetDesc1(&desc))
			&& desc.AdapterLuid.HighPart == selected.AdapterLuid.HighPart
			&& desc.AdapterLuid.LowPart == selected.AdapterLuid.LowPart) {
			return index;
		}
	}

	return UINT_MAX;
}

HRESULT CD3D11Renderer::PresentMediaSample(IMediaSample* sample, const std::function<HRESULT()>& overlay)
{
    if (!sample) {
        return E_POINTER;
    }

    HRESULT hr = DrainPendingFrames(false);
    if (FAILED(hr)) {
        return hr;
    }

    if (m_pendingFrames.size() >= 3) {
        hr = DrainPendingFrames(true);
        if (FAILED(hr)) {
            return hr;
        }
    }

    CComQIPtr<IMediaSampleD3D11> d3d11Sample(sample);
    if (!d3d11Sample) {
        return E_NOINTERFACE;
    }

    D3D11_QUERY_DESC queryDesc = {};
    queryDesc.Query = D3D11_QUERY_EVENT;
    CComPtr<ID3D11Query> completionQuery;
    hr = m_device->CreateQuery(&queryDesc, &completionQuery);
    if (FAILED(hr)) {
        return hr;
    }

    CComPtr<ID3D11Texture2D> texture;
    UINT arraySlice = 0;
    hr = d3d11Sample->GetD3D11Texture(0, &texture, &arraySlice);
    if (FAILED(hr)) {
        return hr;
    }

    hr = SetHDR10MetadataFromSample(sample);
    if (FAILED(hr) && hr != DXGI_ERROR_UNSUPPORTED) {
        return hr;
    }

    hr = PresentD3D11Texture(texture, arraySlice);
    if (FAILED(hr)) {
        return hr;
    }

    if (overlay) {
        if (!m_backBufferRTV) {
            return E_UNEXPECTED;
        }
        m_context->OMSetRenderTargets(1, &m_backBufferRTV.p, nullptr);
        hr = overlay();
        if (FAILED(hr)) {
            return hr;
        }
    }

    hr = Present(0, 0);
    if (FAILED(hr)) {
        return hr;
    }

    m_context->End(completionQuery);
    PendingFrame pending;
    pending.sample = sample;
    pending.query = completionQuery;
    m_pendingFrames.push_back(std::move(pending));
    return S_OK;
}

HRESULT CD3D11Renderer::PresentD3D11Texture(ID3D11Texture2D* texture, UINT arraySlice)
{
    if (!texture || !m_swapChain || !m_context || !m_videoDevice || !m_videoContext) return E_INVALIDARG;

    ScopedDecoderMutex decoderLock(m_decoderMutex);
    if (!decoderLock.Locked()) {
        return E_ACCESSDENIED;
    }
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
    if (textureDevice != m_device) {
        return DXGI_ERROR_INVALID_CALL;
    }
    if (FAILED(textureAdapter->GetDesc(&textureAdapterDesc)) || FAILED(m_adapter->GetDesc(&rendererAdapterDesc)) ||
        textureAdapterDesc.AdapterLuid.HighPart != rendererAdapterDesc.AdapterLuid.HighPart ||
        textureAdapterDesc.AdapterLuid.LowPart != rendererAdapterDesc.AdapterLuid.LowPart) {
        return DXGI_ERROR_DEVICE_REMOVED;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    texture->GetDesc(&textureDesc);
    if (!textureDesc.Width || !textureDesc.Height) return E_INVALIDARG;

    CComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) return hr;

    D3D11_TEXTURE2D_DESC outputDesc2D = {};
    backBuffer->GetDesc(&outputDesc2D);
    if (!outputDesc2D.Width || !outputDesc2D.Height) return E_INVALIDARG;

    D3D11_VIDEO_FRAME_FORMAT frameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
    hr = EnsureVideoProcessor(frameFormat, textureDesc.Width, textureDesc.Height,
        outputDesc2D.Width, outputDesc2D.Height);
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

    RECT sourceRect = { 0, 0, static_cast<LONG>(textureDesc.Width), static_cast<LONG>(textureDesc.Height) };
    RECT destRect = { 0, 0, static_cast<LONG>(outputDesc2D.Width), static_cast<LONG>(outputDesc2D.Height) };

    m_videoContext->VideoProcessorSetStreamSourceRect(
        m_videoProcessor, 0, TRUE, &sourceRect);

    m_videoContext->VideoProcessorSetOutputTargetRect(
        m_videoProcessor, TRUE, &destRect);

    D3D11_VIDEO_PROCESSOR_COLOR_SPACE outputColorSpace = {};
    outputColorSpace.Usage = 0;
    outputColorSpace.RGB_Range = 0;
    outputColorSpace.YCbCr_Matrix = 0;
    outputColorSpace.YCbCr_xvYCC = 0;
    outputColorSpace.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
    m_videoContext->VideoProcessorSetStreamColorSpace(m_videoProcessor, 0, &m_inputColorSpace);\n    m_videoContext->VideoProcessorSetOutputColorSpace(m_videoProcessor, &outputColorSpace);

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
                    // The video processor enumerator is format-dependent.
                    // Recreate it after an SDR/HDR swap-chain transition.
                    m_videoProcessor.Release();
                    m_videoProcessorEnumerator.Release();
                    m_videoWidth = 0;
                    m_videoHeight = 0;
                    m_processorOutputWidth = 0;
                    m_processorOutputHeight = 0;
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
    m_decoderMutex = nullptr;
}
