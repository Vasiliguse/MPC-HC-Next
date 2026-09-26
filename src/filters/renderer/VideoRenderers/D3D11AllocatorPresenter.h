/*
 * D3D11 DirectShow renderer bridge.
 *
 * This filter is intentionally small: DirectShow owns scheduling and
 * graph-state transitions; CD3D11Renderer owns GPU presentation.
 */

#pragma once

#include "AllocatorPresenterImpl.h"
#include "D3D11Renderer.h"
#include "../../transform/MPCVideoDec/D3D11Decoder/ID3DVideoMemoryConfiguration.h"
#include <clsids.h>

namespace DSObjects
{
	class CD3D11VideoRendererFilter;

	class CD3D11RendererInputPin final : public CRendererInputPin, public ID3D11DecoderConfiguration
	{
	public:
		CD3D11RendererInputPin(CD3D11VideoRendererFilter* renderer, HRESULT* phr);
		STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void** ppv) override;
		STDMETHODIMP GetAllocator(IMemAllocator** ppAllocator) override;
		STDMETHODIMP ActivateD3D11Decoding(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HANDLE hMutex, UINT nFlags) override;
		STDMETHODIMP_(UINT) GetD3D11AdapterIndex() override;
	};

	class CD3D11VideoRendererFilter : public CBaseRenderer
	{
	public:
		CD3D11VideoRendererFilter(HWND hWnd, const ExtraRendererSettings& settings, HRESULT* phr);
		~CD3D11VideoRendererFilter() override = default;

		HRESULT CheckMediaType(const CMediaType* pmt) override;
		HRESULT DoRenderSample(IMediaSample* pMediaSample) override;
		HRESULT SetMediaType(const CMediaType* pmt) override;
		CBasePin* GetPin(int n) override;

		HRESULT ActivateD3D11Decoding(ID3D11Device* pDevice, ID3D11DeviceContext* pContext, HANDLE hMutex, UINT nFlags);
		UINT GetD3D11AdapterIndex() const;

	private:
		HWND m_hWnd = nullptr;
		ExtraRendererSettings m_settings = {};
		CD3D11Renderer m_renderer;
		UINT m_sourceWidth = 0;
		UINT m_sourceHeight = 0;
		UINT m_outputWidth = 0;
		UINT m_outputHeight = 0;
	};

	class CD3D11AllocatorPresenter final : public CAllocatorPresenterImpl
	{
	public:
		CD3D11AllocatorPresenter(HWND hWnd, HRESULT& hr, CString& error);
		~CD3D11AllocatorPresenter() override = default;

		STDMETHODIMP CreateRenderer(IUnknown** ppRenderer) override;
		STDMETHODIMP_(CLSID) GetAPCLSID() override;
		STDMETHODIMP_(void) SetPosition(RECT w, RECT v) override;
		STDMETHODIMP_(bool) ResizeDevice() override;
		STDMETHODIMP_(bool) ResetDevice() override;
		STDMETHODIMP_(bool) DisplayChange() override;
		STDMETHODIMP_(void) SetExtraSettings(ExtraRendererSettings* pExtraSets) override;

	private:
		ExtraRendererSettings m_extraSettings = {};
		CComPtr<CD3D11VideoRendererFilter> m_rendererFilter;
	};
}
