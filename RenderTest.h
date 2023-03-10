// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved
//
//----------------------------------------------------------------------------
#pragma once

// Global variables.
extern HINSTANCE g_instance;
extern int g_dpiX;
extern int g_dpiY;
extern float const g_formatWidth;
extern IDWriteFactory* g_dwriteFactory;
extern ID2D1Factory* g_d2dFactory;
extern BOOL g_useARGB32;
extern BOOL g_useWin32Font;
extern BOOL g_showExtents;
extern BOOL g_darkMode;
extern int g_cairoAntialias;
extern int g_cairoSubpixelOrder;
extern int g_scaleX;
extern int g_scaleY;
