// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#pragma once

#include "Common.h"
#include "Settings.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

class FolderModel;

// A layered, top-most, click-through-free overlay window that paints the folder
// contents with Direct2D and is composited with per-pixel alpha (rounded
// corners + translucency) via UpdateLayeredWindow.
class OverlayWindow
{
public:
    bool Initialize(HINSTANCE hInst, FolderModel* model, IWICImagingFactory* wic);
    void Shutdown();

    void Show();
    void Hide();
    void InvalidateIcons();
    void RerenderIfVisible();
    bool IsVisible() const { return visible_; }
    HWND Hwnd() const { return hwnd_; }

    // Live-preview support (used while the settings window is open): measure the
    // panel for a monitor, then show it at an explicit position without moving
    // the cursor or stealing focus, so the settings window stays interactive.
    SIZE MeasureForMonitor(HMONITOR mon);
    void ShowPreviewAt(int x, int y);

    // Live settings.
    void SetHeader(bool showName, bool showCount);
    void SetBackdrop(BackdropStyle backdrop);
    void SetOpacity(float alpha);
    void SetRounded(bool rounded);
    void SetGrid(int columns, int rows);
    void SetCenterCursor(bool center);
    void SetOffset(int offsetX, int offsetY);
    void SetAnimations(bool enabled);

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(UINT, WPARAM, LPARAM);

    void CreateDeviceIndependentResources();
    void CreateDeviceResources();
    void ReleaseDeviceResources();
    void EnsureDIB(int w, int h);
    void ComputeLayout(float scale);
    void Render(float scale);
    void Present();
    void ApplyBackdrop();
    void FinalizeHide();
    int  HitTest(POINT pt) const;
    void OpenItem(int index);
    void ForceForeground();

    HINSTANCE           hInst_ = nullptr;
    HWND                hwnd_  = nullptr;
    FolderModel*        model_ = nullptr;
    IWICImagingFactory* wic_   = nullptr;

    ComPtr<ID2D1Factory>        d2dFactory_;
    ComPtr<ID2D1DCRenderTarget> dcRT_;
    ComPtr<IDWriteFactory>      dwrite_;
    ComPtr<IDWriteTextFormat>   textFormat_;
    ComPtr<IDWriteTextFormat>   titleFormat_;

    ComPtr<ID2D1SolidColorBrush>     brushBg_;
    ComPtr<ID2D1SolidColorBrush>     brushBorder_;
    ComPtr<ID2D1SolidColorBrush>     brushText_;
    ComPtr<ID2D1SolidColorBrush>     brushTitle_;
    ComPtr<ID2D1SolidColorBrush>     brushHover_;
    std::vector<ComPtr<ID2D1Bitmap>> iconCache_;

    // Top-down 32bpp DIB used as the source surface for UpdateLayeredWindow.
    HDC     memDC_      = nullptr;
    HBITMAP dib_        = nullptr;
    HGDIOBJ origBitmap_ = nullptr;
    SIZE    dibSize_    = {0, 0};

    // Layout (computed per Show()).
    std::vector<RECT> itemRects_; // hit-test rects, physical px, panel-relative
    POINT             panelPos_   = {0, 0};
    int               panelPxW_   = 0;
    int               panelPxH_   = 0;
    int               cols_       = 1;
    int               rows_       = 1;
    int               visibleCount_ = 0; // items actually drawn (grid may clip)
    float             panelWDip_  = 0.0f;
    float             panelHDip_  = 0.0f;
    float             gridTopDip_ = 0.0f;
    float             gridLeftDip_= 0.0f;

    // Hover state.
    float scale_      = 1.0f; // DPI scale captured at Show(), reused on re-render
    int   hoverIndex_ = -1;
    bool  tracking_   = false; // WM_MOUSELEAVE tracking armed

    HWND prevForeground_ = nullptr; // window to restore focus to on Hide()

    // Live settings + animation state.
    bool          showName_      = false;
    bool          showCount_     = false;
    BackdropStyle backdrop_      = BackdropStyle::Acrylic;
    float         bgOpacity_     = 0.45f; // panel background alpha
    bool          rounded_       = true;  // rounded panel corners
    int           userCols_      = 0;     // 0 = auto
    int           userRows_      = 0;     // 0 = auto
    bool          centerCursor_  = true;  // place cursor on panel at Show()
    int           offsetX_       = 0;     // open-position nudge, px
    int           offsetY_       = 0;
    bool          animations_    = true;
    float         curAlpha_      = 1.0f; // current window opacity (fade)
    bool          targetVisible_ = false; // fade target

    bool visible_ = false;
    bool preview_ = false; // shown for live preview (settings window open)
};
