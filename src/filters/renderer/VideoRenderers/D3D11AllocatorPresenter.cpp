/*
 * D3D11 DirectShow renderer bridge.
 */

#include "stdafx.h"
#include "D3D11AllocatorPresenter.h"
#include "SubPic/DX11SubPic.h"
#include "SubPic/SubPicQueueImpl.h"
#include <dxva2api.h>

namespace DSObjects
{
	CD3D11RendererInputPin::CD3D11RendererInputPin(CD3D11VideoRendererFilter* renderer, HRESULT* phr)
		: CRendererInputPin(renderer, phr, L"In")
	{
	}

	STDMETHODIMP CD3D11RendererInputPin::GetAllocator(IMemAllocator** ppAllocator)
	{
		if (ppAllocator) {
			*ppAllocator = nullptr;
		}
		return E_FAIL;
	}

	STDMETHODIMP CD3D11RendererInputPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
	{
		CheckPointer(ppv, E_POINTER);
		if (riid == __uuidof(ID3D11DecoderConfiguration)) {
			*ppv = static_cast<ID3D11DecoderConfiguration*>(this);
			CRendererInputPin::AddRef();
			return S_OK;
		}
		return __super::NonDelegatingQueryInterface(riid, ppv);
	}

	STDMETHODIMP CD3D11RendererInputPin::ActivateD3D11Decoding(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HANDLE hMutex, UINT nFlags)
	{
		return static_cast<CD3D11VideoRendererFilter*>(m_pRenderer)->ActivateD3D11Decoding(pDevice, pContext, hMutex, nFlags);
	}

	STDMETHODIMP_(UINT) CD3D11RendererInputPin::GetD3D11AdapterIndex()
	{
		return static_cast<CD3D11VideoRendererFilter*>(m_pRenderer)->GetD3D11AdapterIndex();
	}

	CD3D11VideoRendererFilter::CD3D11VideoRendererFilter(
		HWND hWnd, const ExtraRendererSettings& settings, CD3D11AllocatorPresenter* owner, HRESULT* phr)
		: CBaseRenderer(CLSID_D3D11VideoRenderer, L"MPC D3D11 Video Renderer", nullptr, phr)
		, m_hWnd(hWnd)
		, m_owner(owner)
		, m_settings(settings)
	{
		if (phr && SUCCEEDED(*phr)) {
			*phr = m_renderer.Initialize(m_hWnd, m_settings);
		}
	}

	HRESULT CD3D11VideoRendererFilter::CheckMediaType(const CMediaType* pmt)
	{
		if (!pmt || pmt->majortype != MEDIATYPE_Video) {
			return VFW_E_TYPE_NOT_ACCEPTED;
		}
		return S_OK;
	}

HRESULT CD3D11VideoRendererFilter::ActivateD3D11Decoding(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HANDLE hMutex, UINT nFlags)
{
	HRESULT hr = m_renderer.ActivateD3D11Decoding(pDevice, pContext, hMutex, nFlags);
	if (FAILED(hr)) {
		return hr;
	}
	return m_owner ? m_owner->OnD3D11DeviceActivated(pDevice) : S_OK;
}

UINT CD3D11VideoRendererFilter::GetD3D11AdapterIndex() const
{
	return m_renderer.GetD3D11AdapterIndex();
}

CBasePin* CD3D11VideoRendererFilter::GetPin(int n)
	{
		if (n != 0) {
			return nullptr;
		}

		CAutoLock lock(&m_ObjectCreationLock);
		if (!m_pInputPin) {
			HRESULT hr = S_OK;
			m_pInputPin = DNew CD3D11RendererInputPin(this, &hr);
			if (!m_pInputPin || FAILED(hr)) {
				delete m_pInputPin;
				m_pInputPin = nullptr;
			}
		}
		return m_pInputPin;
	}

HRESULT CD3D11VideoRendererFilter::SetMediaType(const CMediaType* pmt)
	{
		HRESULT hr = __super::SetMediaType(pmt);
		if (FAILED(hr) || !pmt || !pmt->Format()) {
			return hr;
		}

		UINT transferMatrix = 0;
		UINT nominalRange = 0;
		if (pmt->formattype == FORMAT_VideoInfo && pmt->cbFormat >= sizeof(VIDEOINFOHEADER)) {
			const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->Format());
			m_sourceWidth = std::abs(vih->bmiHeader.biWidth);
			m_sourceHeight = std::abs(vih->bmiHeader.biHeight);
		} else if (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2)) {
			const auto* vih = reinterpret_cast<const VIDEOINFOHEADER2*>(pmt->Format());
			m_sourceWidth = std::abs(vih->bmiHeader.biWidth);
			m_sourceHeight = std::abs(vih->bmiHeader.biHeight);

			if (vih->dwControlFlags & AMCONTROL_COLORINFO_PRESENT) {
				DXVA2_ExtendedFormat colorInfo = {};
				colorInfo.value = vih->dwControlFlags;
				transferMatrix = colorInfo.VideoTransferMatrix;
				nominalRange = colorInfo.NominalRange;
			}
		}

		m_renderer.SetInputColorInfo(transferMatrix, nominalRange, m_sourceHeight);
		return S_OK;
	}

	HRESULT CD3D11VideoRendererFilter::DoRenderSample(IMediaSample* pMediaSample)
	{
		if (!pMediaSample) {
			return E_POINTER;
		}

		RECT client = {};
		if (!GetClientRect(m_hWnd, &client)) {
			return HRESULT_FROM_WIN32(GetLastError());
		}

		const UINT width = std::max<LONG>(1, client.right - client.left);
		const UINT height = std::max<LONG>(1, client.bottom - client.top);
		if (width != m_outputWidth || height != m_outputHeight) {
			HRESULT hr = m_renderer.Resize(width, height);
			if (FAILED(hr)) {
				// Native D3D11 decoding owns the device. Never recover resize
				// failures by creating a second renderer-owned device here.
				// DirectShow must reconnect the native decoder and provide its
				// replacement device through ActivateD3D11Decoding().
				return hr;
			}
			m_outputWidth = width;
			m_outputHeight = height;
		}

		// Native D3D11 decoding owns the decoder device. Do not recreate a
		// separate renderer device here after device loss: the decoder must
		// reconnect and call ActivateD3D11Decoding() with its replacement device.
		// Returning the device-loss error lets DirectShow tear down/reconnect
		// the native path instead of presenting against a different device.
		return m_renderer.PresentMediaSample(pMediaSample, [this]() { return m_owner ? m_owner->RenderSubtitles() : S_FALSE; });
	}

	CD3D11AllocatorPresenter::CD3D11AllocatorPresenter(HWND hWnd, HRESULT& hr, CString& error)
		: CAllocatorPresenterImpl(hWnd, hr, &error)
	{
		if (FAILED(hr)) {
			return;
		}
		m_extraSettings.iRendererBackend = VIDEO_RENDERER_BACKEND_D3D11;
	}

	STDMETHODIMP CD3D11AllocatorPresenter::CreateRenderer(IUnknown** ppRenderer)
	{
		CheckPointer(ppRenderer, E_POINTER);
		*ppRenderer = nullptr;

		HRESULT hr = S_OK;
		auto* filter = DNew CD3D11VideoRendererFilter(m_hWnd, m_extraSettings, this, &hr);
		if (!filter) {
			return E_OUTOFMEMORY;
		}
		if (FAILED(hr)) {
			filter->Release();
			return hr;
		}

		m_rendererFilter.Attach(filter);
		return m_rendererFilter->QueryInterface(IID_PPV_ARGS(ppRenderer));
	}

	STDMETHODIMP_(CLSID) CD3D11AllocatorPresenter::GetAPCLSID()
	{
		return CLSID_D3D11AllocatorPresenter;
	}

	HRESULT CD3D11AllocatorPresenter::OnD3D11DeviceActivated(ID3D11Device* device)
	{
		if (!device) return E_POINTER;
		return InitializeSubPicAllocator();
	}

	HRESULT CD3D11AllocatorPresenter::InitializeSubPicAllocator()
	{
		ID3D11Device* device = m_rendererFilter ? m_rendererFilter->GetRendererDevice() : nullptr;
		if (!device) return E_UNEXPECTED;
		CRect client;
		if (!GetClientRect(m_hWnd, &client)) return HRESULT_FROM_WIN32(GetLastError());
		const CSize desktopSize(std::max<LONG>(1, client.Width()), std::max<LONG>(1, client.Height()));
		InitMaxSubtitleTextureSize(m_SubpicSets.iMaxTexWidth, desktopSize);
		if (m_pSubPicAllocator) return m_pSubPicAllocator->ChangeDevice(device);
		m_pSubPicAllocator = DNew CDX11SubPicAllocator(device, m_maxSubtitleTextureSize);
		if (!m_pSubPicAllocator) return E_OUTOFMEMORY;
		m_pSubPicAllocator->SetInverseAlpha(true);
		if (!m_pSubPicQueue) {
			HRESULT hr = S_OK;
			m_pSubPicQueue = (ISubPicQueue*)DNew CSubPicQueueNoThread(!m_SubpicSets.bAnimationWhenBuffering, m_pSubPicAllocator, &hr);
			if (!m_pSubPicQueue || FAILED(hr)) {
				m_pSubPicQueue.Release();
				return FAILED(hr) ? hr : E_FAIL;
			}
			if (m_pSubPicProvider) {
				m_pSubPicQueue->SetSubPicProvider(m_pSubPicProvider);
			}
		}
		return S_OK;
	}

	HRESULT CD3D11AllocatorPresenter::RenderSubtitles()
	{
		if (!m_pSubPicAllocator || !m_pSubPicQueue) return S_FALSE;
		const HRESULT hr = AlphaBltSubPic(m_windowRect, m_videoRect);
		return hr == E_FAIL ? S_FALSE : hr;
	}

	STDMETHODIMP_(void) CD3D11AllocatorPresenter::SetPosition(RECT w, RECT v)
	{
		__super::SetPosition(w, v);
	}

	STDMETHODIMP_(bool) CD3D11AllocatorPresenter::ResizeDevice()
	{
		if (!m_rendererFilter) {
			return false;
		}
		// The renderer resizes lazily on the next frame. Keeping this hook
		// non-destructive avoids racing the DirectShow render thread.
		return true;
	}

	STDMETHODIMP_(bool) CD3D11AllocatorPresenter::ResetDevice()
	{
		// Device recovery is performed by the renderer on the streaming thread,
		// where the failed sample is still available for immediate retry.
		return m_rendererFilter != nullptr;
	}

	STDMETHODIMP_(bool) CD3D11AllocatorPresenter::DisplayChange()
	{
		return ResetDevice();
	}

	STDMETHODIMP_(void) CD3D11AllocatorPresenter::SetExtraSettings(ExtraRendererSettings* pExtraSets)
	{
		if (pExtraSets) {
			m_extraSettings = *pExtraSets;
			m_extraSettings.iRendererBackend = VIDEO_RENDERER_BACKEND_D3D11;
		}
	}

}