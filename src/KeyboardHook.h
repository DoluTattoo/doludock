// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#pragma once

#include "Common.h"
#include "Settings.h"
#include <functional>

// Global low-level keyboard hook that detects a configurable modifier+key
// shortcut, in either "hold" or "toggle" mode, and can also capture a new
// shortcut for the settings UI.
//
// - Hold mode:   onActivate() fires while the chord is held, onDeactivate()
//                when it is released.
// - Toggle mode: onToggle() fires once each time the chord is pressed.
//
// The chord's main key is swallowed so its default action (e.g. the Space
// language-switch) never fires; when WIN is part of the chord, the Start menu
// is suppressed via an inert masking keystroke.
class KeyboardHook
{
public:
    using Callback        = std::function<void()>;
    using CaptureCallback = std::function<void(unsigned mods, unsigned vk, bool cancelled)>;

    bool Install();
    void Uninstall();
    ~KeyboardHook();

    void SetCallbacks(Callback onActivate, Callback onDeactivate, Callback onToggle);
    void Configure(unsigned modifiers, unsigned vk, bool toggleMode);

    // While capturing, the next non-modifier key press is reported through `cb`
    // (with the modifiers held at that moment) instead of activating the
    // overlay; Esc reports a cancellation. Capturing ends automatically.
    void BeginCapture(CaptureCallback cb);
    void EndCapture();
    bool IsCapturing() const { return capturing_; }

private:
    static LRESULT CALLBACK Proc(int code, WPARAM wParam, LPARAM lParam);
    LRESULT HandleEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* info);
    void    UpdateHold();
    void    MaskWinIfNeeded(unsigned mods);

    static KeyboardHook* s_instance;

    HHOOK           hook_ = nullptr;
    Callback        onActivate_;
    Callback        onDeactivate_;
    Callback        onToggle_;
    CaptureCallback captureCb_;

    unsigned requiredMods_ = dk::MOD_WINKEY;
    unsigned hotkeyVk_     = VK_SPACE;
    bool     toggleMode_   = false;

    unsigned mods_          = 0;     // currently-held modifier bitmask
    bool     mainDown_      = false; // main key currently held
    bool     mainSwallowed_ = false; // we swallowed the main key-down
    bool     active_        = false; // hold-mode chord currently satisfied
    bool     masked_        = false; // WIN masking keystroke already sent
    bool     capturing_     = false;
};
