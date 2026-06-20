// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#pragma once

#include "Common.h"

struct App;

// A small, modern settings window: native Win32 controls on a Windows 11 Mica
// backdrop with an immersive dark frame. Created lazily and reused (hidden, not
// destroyed, when closed).
class SettingsWindow
{
public:
    explicit SettingsWindow(App& app);
    ~SettingsWindow();

    void Prepare(); // create + populate (stays hidden)
    void Reveal();  // show and bring to the foreground
    HWND Hwnd() const { return hwnd_; }

    // Updates the "Updates" section status line (called from App after a check).
    void SetUpdateStatus(const std::wstring& text);

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(UINT, WPARAM, LPARAM);

    void EnsureCreated();
    void CreateControls();
    void FinalizeSizeAndScroll();
    void RefreshFromSettings();
    void RefreshHotkeyField();
    void ScrollTo(int pos);

    int Scaled(int v) const { return MulDiv(v, static_cast<int>(dpi_), 96); }

    void OnCommand(int id, int code);
    void BeginHotkeyCapture();
    void EndHotkeyCapture(bool cancelled, unsigned mods, unsigned vk);

    App&  app_;
    HWND  hwnd_       = nullptr;
    HFONT font_       = nullptr;
    HFONT titleFont_  = nullptr;
    HFONT sectionFont_= nullptr;
    HBRUSH fieldBrush_= nullptr;
    HBRUSH ctlBrush_  = nullptr;
    UINT  dpi_        = 96;
    bool  capturing_  = false;

    int   contentH_   = 0; // full content height, px (for scrolling)
    int   scrollY_    = 0; // current vertical scroll offset, px
    int   scrollMax_  = 0; // maximum scroll offset, px

    HWND hHotkeyField_ = nullptr;
    HWND hHotkeyBtn_   = nullptr;
    HWND hToggle_      = nullptr;
    HWND hFolderPath_  = nullptr;
    HWND hShowName_    = nullptr;
    HWND hShowCount_   = nullptr;
    HWND hColumns_     = nullptr;
    HWND hRows_        = nullptr;
    HWND hCenterCursor_= nullptr;
    HWND hPadding_     = nullptr;
    HWND hPaddingVal_  = nullptr;
    HWND hOffsetX_     = nullptr;
    HWND hOffsetY_     = nullptr;
    HWND hOffXVal_     = nullptr;
    HWND hOffYVal_     = nullptr;
    HWND hBackdrop_    = nullptr;
    HWND hOpacity_     = nullptr;
    HWND hOpacityVal_  = nullptr;
    HWND hRounded_     = nullptr;
    HWND hAnim_        = nullptr;
    HWND hAutostart_   = nullptr;
    HWND hUpdateCheck_ = nullptr;
    HWND hUpdateNow_   = nullptr;
    HWND hUpdateStatus_= nullptr;
};
