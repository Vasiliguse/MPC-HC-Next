/*
 * Shared Direct3D 11 media-sample contract.
 *
 * The decoder and renderer use this interface to transfer an already
 * allocated D3D11 texture without forcing a system-memory copy.
 */
#pragma once

#include <d3d11.h>

interface __declspec(uuid("BC8753F5-0AC8-4806-8E5F-A12B2AFE153E"))
IMediaSampleD3D11 : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetD3D11Texture(
        int nView,
        ID3D11Texture2D **ppTexture,
        UINT *pArraySlice) = 0;
};
