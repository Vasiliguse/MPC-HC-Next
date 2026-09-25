/*
 * D3D11 DirectShow renderer bridge.
 */

#include "stdafx.h"
#include "D3D11AllocatorPresenter.h"

namespace DSObjects
{
	CD3D11VideoRendererFilter::CD3D11VideoRendererFilter(
		HWND hWnd, const ExtraRendererSettings& settings, HRESULT* phr)
		: CBaseRenderer(CLSID_D3D11VideoRenderer, L"MPC D3D11 Video Renderer", nullptr, phr)
		, m_hWnd(hWnd)
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

	HRESULT CD3D11VideoRendererFilter::SetMediaType(const CMediaType* pmt)
	{
		HRESULT hr = __super::SetMediaType(pmt);
		if (FAILED(hr) || !pmt || !pmt->Format()) {
			return hr;
		}

		if (pmt->formattype == FORMAT_VideoInfo && pmt->cbFormat >= sizeof(VIDEOINFOHEADER)) {
			const auto* vih = reinterpret_cast<const VIDEOINFOHEADER*>(pmt->Format());
			m_width = std::abs(vih->bmiHeader.biWidth);
			m_height = std::abs(vih->bmiHeader.biHeight);
		} else if (pmt->formattype == FORMAT_VideoInfo2 && pmt->cbFormat >= sizeof(VIDEOINFOHEADER2)) {
			const auto* vih = reinterpret_cast<const VIDEOINFOHEADER2*>(pmt->Format());
			m_width = std::abs(vih->bmiHeader.biWidth);
			m_height = std::abs(vih->bmiHeader.biHeight);
		}

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
		if (width != m_width || height != m_height) {
			HRESULT hr = m_renderer.Resize(width, height);
			if (FAILED(hr) && (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)) {
				hr = m_renderer.Reset();
			}
			if (FAILED(hr)) {
				return hr;
			}
		}

		HRESULT hr = m_renderer.PresentMediaSample(pMediaSample);
		if (FAILED(hr) && m_renderer.IsDeviceLost()) {
			hr = m_renderer.Reset();
			if (SUCCEEDED(hr)) {
				hr = m_renderer.PresentMediaSample(pMediaSample);
			}
		}
		return hr;
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
		auto* filter = DNew CD3D11VideoRendererFilter(m_hWnd, m_extraSettings, &hr);
		if (!filter) {
			return E_OUTOFMEMORY;
		}
		if (FAILED(hr)) {
			filter->Release();
			return hr;
		}

		m_rendererFilter.Attach(filter);
		*ppRenderer = m_rendererFilter;
		(*ppRenderer)->AddRef();
		return S_OK;
	}

	STDMETHODIMP_(CLSID) CD3D11AllocatorPresenter::GetAPCLSID()
	{
		return CLSID_D3D11AllocatorPresenter;
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
		RECT client = {};
		if (!GetClientRect(m_hWnd, &client)) {
			return false;
		}
		HRESULT hr = m_rendererFilter->GetState(0, nullptr);
		UNREFERENCED_PARAMETER(hr);
		// The renderer also resizes lazily on the next frame. This hook is
		// deliberately non-destructive for window-management paths.
		return true;
	}

	STDMETHODIMP_(bool) CD3D11AllocatorPresenter::ResetDevice()
	{
		if (!m_rendererFilter) {
			return false;
		}
		return true;
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
