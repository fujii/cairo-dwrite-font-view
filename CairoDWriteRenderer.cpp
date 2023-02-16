// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
//----------------------------------------------------------------------------

#include "Common.h"
#include "RenderTest.h"
#include "TextHelpers.h"
#include "CairoDWriteRenderer.h"
#include "resource.h"
#include <cairo.h>
#include <cairo-dwrite.h>
#include <cairo-win32.h>
#include <cassert>
#include <vector>

namespace
{
    DWRITE_MATRIX const g_identityTransform =
    {
        1, 0,
        0, 1,
        0, 0
    };
}

IRenderer* CreateCairoDWriteRenderer(
    HWND hwnd,
    UINT width,
    UINT height,
    IDWriteTextFormat* textFormat,
    wchar_t const* text
    )
{
    return new(std::nothrow) CairoDWriteRenderer(
        hwnd,
        width,
        height,
        textFormat,
        text
        );
}

CairoDWriteRenderer::CairoDWriteRenderer(
    HWND hwnd,
    UINT width,
    UINT height,
    IDWriteTextFormat* textFormat,
    wchar_t const* text
    ) : 
    hwnd_(hwnd),
    width_(width), 
    height_(height),
    measuringMode_(DWRITE_MEASURING_MODE_NATURAL),
    transform_(g_identityTransform),
    text_(text),
    borderPen_(NULL),
    textFormat_(SafeAcquire(textFormat)),
    renderTarget_(),
    magnifierTarget_(),
    renderingParams_()
{
    magnifier_.visible = false;
}

void CairoDWriteRenderer::SetFormat(IDWriteTextFormat* format)
{
    SafeSet(&textFormat_, format);
}

void CairoDWriteRenderer::SetText(wchar_t const* text)
{
    text_ = text;
}

void CairoDWriteRenderer::SetMeasuringMode(DWRITE_MEASURING_MODE measuringMode)
{
    measuringMode_ = measuringMode;
}

void CairoDWriteRenderer::SetTransform(DWRITE_MATRIX const& transform)
{
    transform_ = transform;
}

void CairoDWriteRenderer::SetMagnifier(MagnifierInfo const& magnifier)
{
    magnifier_ = magnifier;
    SafeRelease(&magnifierTarget_);
}

void CairoDWriteRenderer::SetWindowSize(UINT width, UINT height)
{
    width_ = width;
    height_ = height;
    SafeRelease(&renderTarget_);

    UpdateTextOrigin();
}

void CairoDWriteRenderer::SetMonitor(HMONITOR monitor)
{
    g_dwriteFactory->CreateMonitorRenderingParams(monitor, &renderingParams_);
}

std::string createUTF8String(const wchar_t* src, size_t srcLength)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, src, srcLength, 0, 0, nullptr, nullptr);
    std::vector<char> buffer(length);
    size_t actualLength = WideCharToMultiByte(CP_UTF8, 0, src, srcLength, buffer.data(), length, nullptr, nullptr);
    return { buffer.data(), actualLength };
}

cairo_matrix_t toCairoMatrix(DWRITE_MATRIX d)
{
    return { d.m11, d.m12, d.m21, d.m22, d.dx, d.dy };
}

static cairo_antialias_t getCairoAntialias()
{
    switch (g_cairoAntialias) {
    case ID_CAIRO_ANTIALIAS_DEFAULT: return CAIRO_ANTIALIAS_DEFAULT;
    case ID_CAIRO_ANTIALIAS_NONE: return CAIRO_ANTIALIAS_NONE;
    case ID_CAIRO_ANTIALIAS_GRAY: return CAIRO_ANTIALIAS_GRAY;
    case ID_CAIRO_ANTIALIAS_SUBPIXEL: return CAIRO_ANTIALIAS_SUBPIXEL;
    case ID_CAIRO_ANTIALIAS_FAST: return CAIRO_ANTIALIAS_FAST;
    case ID_CAIRO_ANTIALIAS_GOOD: return CAIRO_ANTIALIAS_GOOD;
    case ID_CAIRO_ANTIALIAS_BEST: return CAIRO_ANTIALIAS_BEST;
    }
    assert(false);
    return CAIRO_ANTIALIAS_DEFAULT;
}

static cairo_subpixel_order_t getCairoSubpixelOrder()
{
    switch (g_cairoSubpixelOrder) {
    case ID_CAIRO_SUBPIXEL_ORDER_DEFAULT: return CAIRO_SUBPIXEL_ORDER_DEFAULT;
    case ID_CAIRO_SUBPIXEL_ORDER_RGB: return CAIRO_SUBPIXEL_ORDER_RGB;
    case ID_CAIRO_SUBPIXEL_ORDER_BGR: return CAIRO_SUBPIXEL_ORDER_BGR;
    case ID_CAIRO_SUBPIXEL_ORDER_VRGB: return CAIRO_SUBPIXEL_ORDER_VRGB;
    case ID_CAIRO_SUBPIXEL_ORDER_VBGR: return CAIRO_SUBPIXEL_ORDER_VBGR;
    }
    assert(false);
    return CAIRO_SUBPIXEL_ORDER_DEFAULT;
}

void
CairoDWriteRenderer::DrawCairoText(IDWriteBitmapRenderTarget *renderTarget)
{
    DWRITE_MATRIX transform;
    renderTarget->GetCurrentTransform(&transform);
    HDC hdc = renderTarget->GetMemoryDC();

    cairo_format_t format = g_useARGB32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    auto surface = cairo_win32_surface_create_with_format(hdc, format);
    auto cr = cairo_create (surface);

    if (g_darkMode) {
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_paint(cr);
    }

    cairo_surface_set_device_scale (surface, g_dpiX / 96.0f, g_dpiY / 96.0f);

    int PAD = 10;
    cairo_text_extents_t extents;
    cairo_font_extents_t font_extents;

    UINT32 len = textFormat_->GetFontFamilyNameLength();
    std::vector<wchar_t> family_name(len+1);
    textFormat_->GetFontFamilyName(family_name.data(), len+1);

    cairo_font_face_t *face;
    if (!g_useWin32Font) {
	IDWriteFontCollection *systemCollection;
	g_dwriteFactory->GetSystemFontCollection(&systemCollection);

	UINT32 idx;
	BOOL found;
	systemCollection->FindFamilyName(family_name.data(), &idx, &found);

	IDWriteFontFamily *family;
	systemCollection->GetFontFamily(idx, &family);

	IDWriteFont *dwritefont;
	DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
	DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
	family->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, style, &dwritefont);

	IDWriteFontFace *dwriteface;
	dwritefont->CreateFontFace(&dwriteface);

	face = cairo_dwrite_font_face_create_for_dwrite_fontface(dwriteface);

	SafeRelease(&family);
	SafeRelease(&dwritefont);
	SafeRelease(&dwriteface);

	cairo_dwrite_font_face_set_measuring_mode(face, measuringMode_);
	cairo_dwrite_font_face_set_rendering_params(face, renderingParams_);
    } else {
	LOGFONTW log_font = { };
	log_font.lfWeight = FW_REGULAR;
	wcscpy(log_font.lfFaceName, family_name.data());
	face = cairo_win32_font_face_create_for_logfontw(&log_font);
    }

    cairo_set_font_face(cr, face);

    auto fontOptions = cairo_font_options_create();
    cairo_font_options_set_antialias(fontOptions, getCairoAntialias());
    cairo_font_options_set_subpixel_order(fontOptions, getCairoSubpixelOrder());
    cairo_set_font_options(cr, fontOptions);
    cairo_font_options_destroy(fontOptions);

    double y = 0;

    auto matrix = toCairoMatrix(transform);
    cairo_set_matrix(cr, &matrix);
    cairo_set_font_size (cr, textFormat_->GetFontSize());

    auto str = createUTF8String(text_, lstrlen(text_));
    cairo_text_extents (cr, str.c_str(), &extents);
    cairo_translate (cr,
		     PAD - extents.x_bearing,
		     PAD - extents.y_bearing + y);

    if (g_showExtents) {
	cairo_font_extents (cr, &font_extents);
	cairo_rectangle (cr, 0, -font_extents.ascent,
			 extents.x_advance, font_extents.height);
	cairo_move_to (cr, -PAD, 0);
	cairo_line_to (cr, extents.width + PAD, 0);
	cairo_set_source_rgba (cr, 0, 0, 1, .7);
	cairo_stroke (cr);

	cairo_rectangle (cr,
			 extents.x_bearing, extents.y_bearing,
			 extents.width, extents.height);
	cairo_set_source_rgba (cr, 0.5, 1, 0, .7);
	cairo_stroke (cr);
    }
    
    cairo_move_to (cr, 0, 0);
    if (g_darkMode)
	cairo_set_source_rgb (cr, 1, 1, 1);
    else
	cairo_set_source_rgb (cr, 0, 0, 0);
    cairo_show_text (cr, str.c_str());

    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}

HRESULT CairoDWriteRenderer::Draw(HDC hdc)
{
    HRESULT hr = S_OK;

    // Create the bitmap render target if we don't already have it.
    if (renderTarget_ == NULL)
    {
        IDWriteGdiInterop* gdiInterop = NULL;

        if (SUCCEEDED(hr))
        {
            hr = g_dwriteFactory->GetGdiInterop(&gdiInterop);
        }
        if (SUCCEEDED(hr))
        {
            hr = gdiInterop->CreateBitmapRenderTarget(hdc, width_, height_, &renderTarget_);
        }
        if (SUCCEEDED(hr))
        {
            hr = renderTarget_->SetPixelsPerDip(g_dpiY / 96.0f);
        }

        SafeRelease(&gdiInterop);
    }

    // Create the rendering params object if we haven't already.
    if (SUCCEEDED(hr) && renderingParams_ == NULL)
    {
        hr = g_dwriteFactory->CreateRenderingParams(&renderingParams_);
    }

    HDC hdcMem = NULL;
    if (SUCCEEDED(hr))
    {
        // Clear the background.
        hdcMem = renderTarget_->GetMemoryDC();
        SelectObject(hdcMem, GetSysColorBrush(COLOR_WINDOW));
        PatBlt(hdcMem, 0, 0, width_, height_, PATCOPY);

        // Set the rendering transform.
        renderTarget_->SetCurrentTransform(&transform_);

        // Prepare the render target we use for the magnifier.
        PrepareMagnifier(hdc);
    }

    if (SUCCEEDED(hr))
    {
	DrawCairoText(renderTarget_);
	DrawCairoText(magnifierTarget_);
    }

    if (SUCCEEDED(hr))
    {
        DrawMagnifier();

        // Do the final BitBlt to the specified HDC.
        BitBlt(hdc, 0, 0, width_, height_, hdcMem, 0, 0, SRCCOPY | NOMIRRORBITMAP);
    }

    return hr;
}

HRESULT CairoDWriteRenderer::PrepareMagnifier(HDC hdc)
{
    HRESULT hr = S_OK;

    if (!magnifier_.visible)
    {
        SafeRelease(&magnifierTarget_);
        return hr;
    }

    // Determine the size and scale factor for the magnifier render target. In vector
    // mode we render using a scale transform. In all other modes we render at normal
    // size and then scale up the pixels afterwards.
    SIZE targetSize = magnifier_.magnifierSize;
    int targetScale = magnifier_.scale;
    if (magnifier_.type != MagnifierInfo::Vector)
    {
        targetSize.cx /= targetScale;
        targetSize.cy /= targetScale;
        targetScale = 1;
    }

    // Create a separate render target for the magnifier if we haven't already.
    if (SUCCEEDED(hr) && magnifierTarget_ == NULL)
    {
        IDWriteGdiInterop* gdiInterop = NULL;

        if (SUCCEEDED(hr))
        {
            hr = g_dwriteFactory->GetGdiInterop(&gdiInterop);
        }
        if (SUCCEEDED(hr))
        {
            hr = gdiInterop->CreateBitmapRenderTarget(hdc, targetSize.cx, targetSize.cy, &magnifierTarget_);
        }

        SafeRelease(&gdiInterop);
    }

    DWRITE_MATRIX zoomTransform;;
    if (SUCCEEDED(hr))
    {
        // Clear the background.
        HDC hdcMagnifier = magnifierTarget_->GetMemoryDC();
        SelectObject(hdcMagnifier, GetSysColorBrush(COLOR_WINDOW));
        PatBlt(hdcMagnifier, 0, 0, magnifier_.magnifierSize.cx, magnifier_.magnifierSize.cy, PATCOPY);

        // Create a transform that translates and scales the focus rect to the origin of the magnifier target.
        float focusLeft = PixelsToDipsX(magnifier_.focusPos.x);
        float focusTop = PixelsToDipsY(magnifier_.focusPos.y);

        zoomTransform.m11 = transform_.m11 * targetScale;
        zoomTransform.m12 = transform_.m12 * targetScale;
        zoomTransform.m21 = transform_.m21 * targetScale;
        zoomTransform.m22 = transform_.m22 * targetScale;
        zoomTransform.dx = (transform_.dx - focusLeft) * targetScale;
        zoomTransform.dy = (transform_.dy - focusTop) * targetScale;
    }

    if (SUCCEEDED(hr))
    {
        hr = magnifierTarget_->SetCurrentTransform(&zoomTransform);
    }

    return hr;
}

void CairoDWriteRenderer::DrawMagnifier()
{
    if (magnifierTarget_ == NULL)
        return;

    HDC memoryDC = renderTarget_->GetMemoryDC();

    // Copy the text from the magnifier render target to the main render target.
    switch (magnifier_.type)
    {
    case MagnifierInfo::Vector:
        // We rendered the text at the larger scale; just copy it.
        BitBlt(
            memoryDC, 
            magnifier_.magnifierPos.x, 
            magnifier_.magnifierPos.y,
            magnifier_.magnifierSize.cx,
            magnifier_.magnifierSize.cy,
            magnifierTarget_->GetMemoryDC(),
            0,
            0,
            SRCCOPY | NOMIRRORBITMAP
            );
        break;

    case MagnifierInfo::Pixel:
        // We rendered the text at normal size; copy and scale up.
        StretchBlt(
            memoryDC, 
            magnifier_.magnifierPos.x, 
            magnifier_.magnifierPos.y,
            magnifier_.magnifierSize.cx,
            magnifier_.magnifierSize.cy,
            magnifierTarget_->GetMemoryDC(),
            0,
            0,
            magnifier_.magnifierSize.cx / magnifier_.scale,
            magnifier_.magnifierSize.cy / magnifier_.scale,
            SRCCOPY | NOMIRRORBITMAP
            );
        break;

    case MagnifierInfo::Subpixel:
        SubpixelZoom();
        break;
    }

    // Draw the borders around the magnifier and focus rectangle.
    if (borderPen_ == NULL)
    {
        borderPen_ = CreatePen(PS_SOLID, 1, RGB(255, 0, 0));
    }

    HGDIOBJ oldBrush = SelectObject(memoryDC, GetStockObject(NULL_BRUSH));
    HGDIOBJ oldPen = SelectObject(memoryDC, borderPen_);

    Rectangle(
        memoryDC, 
        magnifier_.magnifierPos.x, 
        magnifier_.magnifierPos.y, 
        magnifier_.magnifierPos.x + magnifier_.magnifierSize.cx,
        magnifier_.magnifierPos.y + magnifier_.magnifierSize.cy
        );

    Rectangle(
        memoryDC, 
        magnifier_.focusPos.x,
        magnifier_.focusPos.y, 
        magnifier_.focusPos.x + magnifier_.magnifierSize.cx / magnifier_.scale,
        magnifier_.focusPos.y + magnifier_.magnifierSize.cy / magnifier_.scale
        );

    SelectObject(memoryDC, oldPen);
    SelectObject(memoryDC, oldBrush);
}

void CairoDWriteRenderer::SubpixelZoom()
{
    bool bgr = renderingParams_->GetPixelGeometry() == DWRITE_PIXEL_GEOMETRY_BGR;

    // Get the DIB selection selected into each IDWriteBitmapRenderTarget's memory DC.
    DIBSECTION srcDib;
    if (GetObject(GetCurrentObject(magnifierTarget_->GetMemoryDC(), OBJ_BITMAP), sizeof(srcDib), &srcDib) != sizeof(srcDib))
        return;

    DIBSECTION dstDib;
    if (GetObject(GetCurrentObject(renderTarget_->GetMemoryDC(), OBJ_BITMAP), sizeof(dstDib), &dstDib) != sizeof(dstDib))
        return;

    // Point to the pixels. Each DIB section is a 32-bit per pixel top-down DIB.
    int const srcWidth = srcDib.dsBm.bmWidth;
    int const srcHeight = srcDib.dsBm.bmHeight;
    UINT32 const* const srcBits = static_cast<UINT32*>(srcDib.dsBm.bmBits);

    int const dstWidth = dstDib.dsBm.bmWidth;
    int const dstHeight = dstDib.dsBm.bmHeight;
    UINT32* const dstBits = static_cast<UINT32*>(dstDib.dsBm.bmBits);

    // Number of target pixels per source pixel and source subpixel.
    int const scale = magnifier_.scale;
    int const subpixelScale = scale / 3;
    int const pixelGap = scale % 3;

    // Mask of colors for left, center, and right subpixels.
    UINT32 const maskL = bgr ? 0x0000FF : 0xFF0000;
    UINT32 const maskC = 0x00FF00;
    UINT32 const maskR = bgr ? 0xFF0000 : 0x0000FF;

    // Iterate over all the source scan lines.
    for (int y = 0; y < srcHeight; ++y)
    {
        UINT32 const* srcRow = srcBits + (y * srcWidth);

        // Determine the corresponding range of Y values in the destination bitmap.
        int minDstY = (y * scale) + magnifier_.magnifierPos.y;
        int limDstY = minDstY + scale;

        // Consrain the destination Y values to fit in the destination bitmap.
        if (minDstY < 0)
            minDstY = 0;

        if (limDstY > dstHeight)
            limDstY = dstHeight;

        // Are any of the destination scan lines visible?
        if (minDstY < limDstY)
        {
            UINT32* const firstDstRow = dstBits + (minDstY * dstWidth);

            int dstX = magnifier_.magnifierPos.x;

            // Iterate over all the pixels in the source scan line.
            for (int x = 0; x < srcWidth; ++x)
            {
                UINT32 const color = srcRow[x];

                // Fill in the destination pixels for the left, center,
                // and right color stripes.
                for (int i = 0; i < subpixelScale; ++i, ++dstX)
                {
                    if (dstX >= 0 && dstX < dstWidth)
                        firstDstRow[dstX] = color & maskL;
                }
                for (int i = 0; i < subpixelScale; ++i, ++dstX)
                {
                    if (dstX >= 0 && dstX < dstWidth)
                        firstDstRow[dstX] = color & maskC;
                }
                for (int i = 0; i < subpixelScale; ++i, ++dstX)
                {
                    if (dstX >= 0 && dstX < dstWidth)
                        firstDstRow[dstX] = color & maskR;
                }

                // If the scale is not a multiple if three, we'll have a black
                // gap between the pixels.
                for (int i = 0; i < pixelGap; ++i, ++dstX)
                {
                    if (dstX >= 0 && dstX < dstWidth)
                        firstDstRow[dstX] = 0;
                }
            }

            // Copy the destination row we just initialized to the remaining 
            // destination rows for this scan line.
            UINT32* dstRow = firstDstRow + dstWidth;

            for (int y2 = minDstY + 1; y2 < limDstY; ++y2, dstRow += dstWidth)
            {
                memcpy(
                    dstRow + magnifier_.magnifierPos.x,
                    firstDstRow + magnifier_.magnifierPos.x,
                    (dstX - magnifier_.magnifierPos.x) * sizeof(UINT32)
                    );
            }
        }
    }
}

HRESULT CairoDWriteRenderer::UpdateTextOrigin()
{
    HRESULT hr = S_OK;

    // Get the text layout size.
    DWRITE_TEXT_METRICS metrics = {};

    // Center the text.
    textOriginX_ = (PixelsToDipsX(width_) - metrics.width) * 0.5f;
    textOriginY_ = (PixelsToDipsY(height_) - metrics.height) * 0.5f;

    return hr;
}

//
// IUnknown methods
//
//      These methods are never called in this scenario so we just use stub
//      implementations.
//
HRESULT STDMETHODCALLTYPE CairoDWriteRenderer::QueryInterface( 
    REFIID riid,
    void** ppvObject
    )
{
    *ppvObject = NULL;
    return E_NOTIMPL;
}

ULONG STDMETHODCALLTYPE CairoDWriteRenderer::AddRef()
{
    return 0;
}

ULONG STDMETHODCALLTYPE CairoDWriteRenderer::Release()
{
    return 0;
}
