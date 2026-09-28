#include "stdafx.h"
#include "D3D12Renderer.h"

#include <algorithm>

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
           hr == DXGI_ERROR_DEVICE_HUNG || hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
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

    hr = CreateFrameResources();
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

    hr = CreateRenderTargetViews();
    if (FAILED(hr)) {
        return hr;
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

    IUnknown* queues[] = { m_commandQueue };
    hr = D3D11On12CreateDevice(
        m_device,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        queues,
        1,
        0,
        &m_d3d11On12Device,
        &m_d3d11On12Context,
        nullptr);
    if (FAILED(hr)) {
        m_d3d11On12Device.Release();
        m_d3d11On12Context.Release();
        return hr;
    }

    hr = m_d3d11On12Device->QueryInterface(IID_PPV_ARGS(&m_d3d11On12Device2));
    if (FAILED(hr)) {
        m_d3d11On12Device.Release();
        m_d3d11On12Context.Release();
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

HRESULT CD3D12Renderer::CreateRenderTargetViews()
{
    if (!m_device || !m_swapChain) {
        return E_UNEXPECTED;
    }

    m_rtvHeap.Release();

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = kBufferCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    HRESULT hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; ++i) {
        CComPtr<ID3D12Resource> buffer;
        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer));
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
        m_device->CreateRenderTargetView(buffer, nullptr, handle);
        handle.ptr += m_rtvDescriptorSize;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    return S_OK;
}

HRESULT CD3D12Renderer::CreateFrameResources()
{
    if (!m_device || !m_commandQueue || !m_swapChain) {
        return E_UNEXPECTED;
    }

    m_commandAllocators.clear();
    m_commandList.Release();
    m_fence.Release();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_commandAllocators.resize(kBufferCount);
    m_frameFenceValues.assign(kBufferCount, 0);
    for (auto& allocator : m_commandAllocators) {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
    }

    HRESULT hr = m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0], nullptr,
        IID_PPV_ARGS(&m_commandList));
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_commandList->Close();
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    m_fenceValue = 0;
    return S_OK;
}

HRESULT CD3D12Renderer::SignalAndWait()
{
    if (!m_commandQueue || !m_fence || !m_fenceEvent) {
        return E_UNEXPECTED;
    }

    const UINT64 value = ++m_fenceValue;
    HRESULT hr = m_commandQueue->Signal(m_fence, value);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    if (m_fence->GetCompletedValue() < value) {
        hr = m_fence->SetEventOnCompletion(value, m_fenceEvent);
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
        if (WaitForSingleObject(m_fenceEvent, INFINITE) != WAIT_OBJECT_0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    return S_OK;
}

HRESULT CD3D12Renderer::WaitForGpu()
{
    return SignalAndWait();
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

    HRESULT hr = WaitForGpu();
    if (FAILED(hr)) {
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        return hr;
    }

    HRESULT hr2 = m_swapChain->ResizeBuffers(kBufferCount, width, height, m_swapChainFormat, flags);
    if (IsDeviceLostHr(hr2)) {
        m_deviceLost = true;
    }
    if (FAILED(hr2)) {
        return hr2;
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
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
        m_swapChainFormat = desiredFormat;
    }

    hr = CreateRenderTargetViews();
    if (FAILED(hr)) {
        return hr;
    }

    return ConfigureSwapChainColorSpace();
}

bool CD3D12Renderer::IsAdapterCompatible(ID3D11Device* device) const
{
    if (!device || !m_adapter) {
        return false;
    }

    CComPtr<IDXGIDevice> dxgiDevice;
    CComPtr<IDXGIAdapter> decoderAdapter;
    DXGI_ADAPTER_DESC decoderDesc = {};
    DXGI_ADAPTER_DESC1 rendererDesc = {};

    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) ||
        FAILED(dxgiDevice->GetAdapter(&decoderAdapter)) ||
        FAILED(decoderAdapter->GetDesc(&decoderDesc)) ||
        FAILED(m_adapter->GetDesc1(&rendererDesc))) {
        return false;
    }

    return decoderDesc.AdapterLuid.HighPart == rendererDesc.AdapterLuid.HighPart
        && decoderDesc.AdapterLuid.LowPart == rendererDesc.AdapterLuid.LowPart;
}

HRESULT CD3D12Renderer::OpenSharedD3D11Texture(ID3D11Texture2D* texture, ID3D12Resource** resource)
{
    if (!texture || !resource || !m_device) {
        return E_INVALIDARG;
    }
    *resource = nullptr;

    CComPtr<ID3D11Device> textureDevice;
    texture->GetDevice(&textureDevice);
    if (!textureDevice) {
        return E_FAIL;
    }
    HRESULT hr = S_OK;

    if (!IsAdapterCompatible(textureDevice)) {
        return DXGI_ERROR_UNSUPPORTED;
    }

    D3D11_TEXTURE2D_DESC textureDesc = {};
    texture->GetDesc(&textureDesc);
    if ((textureDesc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) == 0) {
        // D3D12::OpenSharedHandle consumes NT handles. Legacy D3D11 shared
        // resources use the older GetSharedHandle path and cannot be passed
        // to this importer safely.
        return DXGI_ERROR_UNSUPPORTED;
    }

    CComPtr<IDXGIResource1> sharedResource;
    hr = texture->QueryInterface(IID_PPV_ARGS(&sharedResource));
    if (FAILED(hr)) {
        return hr;
    }

    HANDLE sharedHandle = nullptr;
    hr = sharedResource->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &sharedHandle);
    if (FAILED(hr)) {
        // The decoder currently uses the legacy D3D11_RESOURCE_MISC_SHARED
        // path. CreateSharedHandle intentionally fails for that resource type;
        // keep this as a capability gate until the decoder surfaces are moved
        // to D3D11_RESOURCE_MISC_SHARED_NTHANDLE.
        return hr;
    }

    hr = m_device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(resource));
    CloseHandle(sharedHandle);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    return hr;
}

HRESULT CD3D12Renderer::OpenSharedD3D11Fence(ID3D11Fence* fence, ID3D12Fence** sharedFence)
{
    if (!fence || !sharedFence || !m_device) {
        return E_INVALIDARG;
    }
    *sharedFence = nullptr;

    CComPtr<ID3D11Device> fenceDevice;
    fence->GetDevice(&fenceDevice);
    if (!fenceDevice) {
        return E_FAIL;
    }
    if (!IsAdapterCompatible(fenceDevice)) {
        return DXGI_ERROR_UNSUPPORTED;
    }

    HANDLE sharedHandle = nullptr;
    HRESULT hr = fence->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &sharedHandle);
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_device->OpenSharedHandle(sharedHandle, IID_PPV_ARGS(sharedFence));
    CloseHandle(sharedHandle);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    return hr;
}

HRESULT CD3D12Renderer::PresentTexture(ID3D12Resource* source, D3D12_RESOURCE_STATES sourceState, ID3D12Fence* waitFence, UINT64 waitValue)
{
    if (!source || !m_swapChain || !m_device || !m_commandQueue ||
        m_commandAllocators.empty() || !m_commandList || !m_rtvHeap ||
        !m_fence || !m_fenceEvent || m_frameFenceValues.size() != m_commandAllocators.size()) {
        return E_UNEXPECTED;
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    if (m_frameIndex >= m_commandAllocators.size()) {
        return E_UNEXPECTED;
    }

    auto& allocator = m_commandAllocators[m_frameIndex];
    HRESULT hr = S_OK;
    if (m_frameFenceValues[m_frameIndex] != 0 &&
        m_fence->GetCompletedValue() < m_frameFenceValues[m_frameIndex]) {
        hr = m_fence->SetEventOnCompletion(m_frameFenceValues[m_frameIndex], m_fenceEvent);
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
        if (WaitForSingleObject(m_fenceEvent, INFINITE) != WAIT_OBJECT_0) {
            return HRESULT_FROM_WIN32(GetLastError());
        }
    }

    hr = allocator->Reset();
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    hr = m_commandList->Reset(allocator, nullptr);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    CComPtr<ID3D12Resource> backBuffer;
    hr = m_swapChain->GetBuffer(m_frameIndex, IID_PPV_ARGS(&backBuffer));
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    const D3D12_RESOURCE_DESC sourceDesc = source->GetDesc();
    const D3D12_RESOURCE_DESC targetDesc = backBuffer->GetDesc();
    if (sourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        targetDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        sourceDesc.Format != targetDesc.Format ||
        sourceDesc.Width != targetDesc.Width ||
        sourceDesc.Height != targetDesc.Height) {
        m_commandList->Close();
        return DXGI_ERROR_UNSUPPORTED;
    }

    D3D12_RESOURCE_BARRIER barriers[3] = {};
    UINT initialBarrierCount = 0;

    if (sourceState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        barriers[initialBarrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[initialBarrierCount].Transition.pResource = source;
        barriers[initialBarrierCount].Transition.StateBefore = sourceState;
        barriers[initialBarrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[initialBarrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        ++initialBarrierCount;
    }

    barriers[initialBarrierCount].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[initialBarrierCount].Transition.pResource = backBuffer;
    barriers[initialBarrierCount].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[initialBarrierCount].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[initialBarrierCount].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    ++initialBarrierCount;

    m_commandList->ResourceBarrier(initialBarrierCount, barriers);
    m_commandList->CopyResource(backBuffer, source);

    if (sourceState != D3D12_RESOURCE_STATE_COPY_SOURCE) {
        D3D12_RESOURCE_BARRIER sourceRestore = {};
        sourceRestore.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        sourceRestore.Transition.pResource = source;
        sourceRestore.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        sourceRestore.Transition.StateAfter = sourceState;
        sourceRestore.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_commandList->ResourceBarrier(1, &sourceRestore);
    }

    D3D12_RESOURCE_BARRIER presentBarrier = {};
    presentBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    presentBarrier.Transition.pResource = backBuffer;
    presentBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    presentBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    presentBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &presentBarrier);

    hr = m_commandList->Close();
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }

    if (waitFence) {
        hr = m_commandQueue->Wait(waitFence, waitValue);
        if (IsDeviceLostHr(hr)) {
            m_deviceLost = true;
        }
        if (FAILED(hr)) {
            return hr;
        }
    }

    ID3D12CommandList* lists[] = { m_commandList };
    m_commandQueue->ExecuteCommandLists(1, lists);

    if (m_device) {
        const HRESULT deviceReason = m_device->GetDeviceRemovedReason();
        if (IsDeviceLostHr(deviceReason)) {
            m_deviceLost = true;
            return deviceReason;
        }
    }

    hr = Present(0);
    if (FAILED(hr)) {
        return hr;
    }

    const UINT64 fenceValue = ++m_fenceValue;
    hr = m_commandQueue->Signal(m_fence, fenceValue);
    if (IsDeviceLostHr(hr)) {
        m_deviceLost = true;
    }
    if (FAILED(hr)) {
        return hr;
    }
    m_frameFenceValues[m_frameIndex] = fenceValue;
    return S_OK;
}

HRESULT CD3D12Renderer::PresentD3D11Texture(ID3D11Texture2D* texture, ID3D11Fence* fence, UINT64 fenceValue)
{
    if (!texture || !fence || fenceValue == 0 || !m_d3d11On12Device2) {
        return E_INVALIDARG;
    }

    CComPtr<ID3D12Resource> resource;
    HRESULT hr = m_d3d11On12Device2->UnwrapUnderlyingResource(
        texture,
        m_commandQueue,
        IID_PPV_ARGS(&resource));
    if (FAILED(hr)) {
        if (IsDeviceLostHr(hr)) m_deviceLost = true;
        return hr;
    }

    // UnwrapUnderlyingResource places the native D3D11 resource in COMMON and
    // schedules translation-layer waits. The native decoder's own fence is
    // still supplied because its work is produced by a separate D3D11 device.
    hr = PresentTexture(
        resource,
        D3D12_RESOURCE_STATE_COMMON,
        m_sharedFence ? m_sharedFence.p : nullptr,
        fenceValue);
    if (FAILED(hr)) {
        return hr;
    }

    const UINT64 completionValue = m_fenceValue;
    ID3D12Fence* completionFence = m_fence;
    hr = m_d3d11On12Device2->ReturnUnderlyingResource(
        texture,
        1,
        &completionValue,
        &completionFence);
    if (FAILED(hr)) {
        if (IsDeviceLostHr(hr)) m_deviceLost = true;
        return hr;
    }

    return S_OK;
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
    if (m_commandQueue && m_fence && m_fenceEvent) {
        SignalAndWait();
    }

    m_commandList.Release();
    m_commandAllocators.clear();
    m_frameFenceValues.clear();
    m_rtvHeap.Release();
    m_fence.Release();
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    m_sharedInputTexture.Release();
    m_sharedInputResource.Release();
    m_sharedInputFence.Release();
    m_sharedFence.Release();
    m_d3d11On12Device2.Release();
    m_d3d11On12Context.Release();
    m_d3d11On12Device.Release();

    m_swapChain.Release();
    m_commandQueue.Release();
    m_device.Release();
    m_output6.Release();
    m_adapter.Release();
    m_factory.Release();
    m_output = {};
    m_deviceLost = false;
}
