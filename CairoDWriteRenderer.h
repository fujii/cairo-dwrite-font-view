// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
//----------------------------------------------------------------------------
#pragma once
#include "IRenderer.h"

class CairoDWriteRenderer : public IRenderer
{
public:
    CairoDWriteRenderer(
        HWND hwnd,
        UINT width,
        UINT height,
        IDWriteTextFormat* textFormat,
        wchar_t const* text
        );

    ~CairoDWriteRenderer()
    {
        SafeRelease(&textFormat_);
        SafeRelease(&renderingParams_);
        SafeRelease(&renderTarget_);
        SafeRelease(&magnifierTarget_);
        DeleteObject(borderPen_);
    }

    virtual void SetFormat(IDWriteTextFormat* format);
    virtual void SetText(wchar_t const* text);
    virtual void SetWindowSize(UINT width, UINT height);
    virtual void SetMonitor(HMONITOR monitor);
    virtual void SetMeasuringMode(DWRITE_MEASURING_MODE measuringMode);
    virtual void SetTransform(DWRITE_MATRIX const& transform);
    virtual void SetMagnifier(MagnifierInfo const& magnifier);

    virtual HRESULT Draw(HDC hdc);

private:

    // IUnknown methods
    IFACEMETHOD(QueryInterface)( 
        REFIID riid,
        void** ppvObject
        );
    IFACEMETHOD_(ULONG, AddRef)();
    IFACEMETHOD_(ULONG, Release)();

private:
    HRESULT PrepareMagnifier(HDC hdc);
    void DrawCairoText(IDWriteBitmapRenderTarget*);
    void DrawMagnifier();
    void SubpixelZoom();

    HRESULT UpdateTextOrigin();

    HWND hwnd_;
    UINT width_;
    UINT height_;
    DWRITE_MEASURING_MODE measuringMode_;
    DWRITE_MATRIX transform_;
    MagnifierInfo magnifier_;
    IDWriteTextFormat* textFormat_;
    __nullterminated wchar_t const* text_;

    // Rendering interfaces.
    IDWriteBitmapRenderTarget* renderTarget_;
    IDWriteBitmapRenderTarget* magnifierTarget_;
    IDWriteRenderingParams* renderingParams_;
    HPEN borderPen_;

    // Text layout.
    float textOriginX_;
    float textOriginY_;
};
