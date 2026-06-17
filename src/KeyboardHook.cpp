// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#include "KeyboardHook.h"

KeyboardHook* KeyboardHook::s_instance = nullptr;

namespace
{
// Taps an inert key (Ctrl) so Windows considers the WIN key "used" and does not
// open the Start menu when WIN is released. This mirrors the masking trick used
// by AutoHotkey for WIN-key hotkeys.
void SendMaskKey()
{
    INPUT in[2] = {};
    in[0].type       = INPUT_KEYBOARD;
    in[0].ki.wVk     = VK_CONTROL;
    in[1].type       = INPUT_KEYBOARD;
    in[1].ki.wVk     = VK_CONTROL;
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

// Maps a virtual-key code to its modifier bit, or 0 if it is not a modifier.
unsigned ModifierBit(DWORD vk)
{
    switch (vk)
    {
    case VK_LWIN: case VK_RWIN:
        return dk::MOD_WINKEY;
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        return dk::MOD_CTRLKEY;
    case VK_MENU: case VK_LMENU: case VK_RMENU:
        return dk::MOD_ALTKEY;
    case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        return dk::MOD_SHIFTKEY;
    default:
        return 0;
    }
}
} // namespace

bool KeyboardHook::Install()
{
    s_instance = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &KeyboardHook::Proc, GetModuleHandleW(nullptr), 0);
    return hook_ != nullptr;
}

void KeyboardHook::Uninstall()
{
    if (hook_)
    {
        UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (s_instance == this)
        s_instance = nullptr;
}

KeyboardHook::~KeyboardHook()
{
    Uninstall();
}

void KeyboardHook::SetCallbacks(Callback onActivate, Callback onDeactivate, Callback onToggle)
{
    onActivate_   = std::move(onActivate);
    onDeactivate_ = std::move(onDeactivate);
    onToggle_     = std::move(onToggle);
}

void KeyboardHook::Configure(unsigned modifiers, unsigned vk, bool toggleMode)
{
    requiredMods_  = modifiers;
    hotkeyVk_      = vk;
    toggleMode_    = toggleMode;
    active_        = false;
    mainDown_      = false;
    mainSwallowed_ = false;
}

void KeyboardHook::BeginCapture(CaptureCallback cb)
{
    captureCb_ = std::move(cb);
    capturing_ = true;
}

void KeyboardHook::EndCapture()
{
    capturing_ = false;
    captureCb_ = nullptr;
}

LRESULT CALLBACK KeyboardHook::Proc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && s_instance)
    {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        // Ignore our own injected masking keystrokes to avoid recursion.
        if (!(info->flags & LLKHF_INJECTED))
        {
            const LRESULT r = s_instance->HandleEvent(wParam, info);
            if (r)
                return r; // swallow the keystroke
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

LRESULT KeyboardHook::HandleEvent(WPARAM wParam, const KBDLLHOOKSTRUCT* info)
{
    const bool     isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    const bool     isUp   = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    const DWORD    vk     = info->vkCode;
    const unsigned bit    = ModifierBit(vk);

    if (bit)
    {
        if (isDown)
            mods_ |= bit;
        else if (isUp)
            mods_ &= ~bit;

        // Re-arm the WIN mask for the next chord once WIN is released.
        if (bit == dk::MOD_WINKEY && isUp)
            masked_ = false;

        if (!capturing_ && !toggleMode_)
            UpdateHold();
        return 0; // never swallow modifiers themselves
    }

    // --- non-modifier key ---------------------------------------------------

    if (capturing_)
    {
        if (isDown)
        {
            if (vk == VK_ESCAPE)
            {
                auto cb    = captureCb_;
                capturing_ = false;
                captureCb_ = nullptr;
                if (cb)
                    cb(0, 0, true);
            }
            else
            {
                const unsigned m  = mods_;
                auto           cb = captureCb_;
                capturing_        = false;
                captureCb_        = nullptr;
                MaskWinIfNeeded(m);
                if (cb)
                    cb(m, vk, false);
            }
        }
        return 1; // swallow keys while capturing
    }

    if (vk == hotkeyVk_)
    {
        const bool modsMatch = (mods_ == requiredMods_);

        if (toggleMode_)
        {
            if (isDown)
            {
                if (modsMatch && !mainDown_)
                {
                    mainDown_      = true;
                    mainSwallowed_ = true;
                    MaskWinIfNeeded(requiredMods_);
                    if (onToggle_)
                        onToggle_();
                    return 1;
                }
                if (mainDown_)
                    return 1; // swallow auto-repeat
            }
            else if (isUp)
            {
                const bool swallowed = mainSwallowed_;
                mainDown_            = false;
                mainSwallowed_       = false;
                if (swallowed)
                    return 1;
            }
        }
        else // hold mode
        {
            if (isDown)
            {
                if (modsMatch)
                {
                    mainDown_      = true;
                    mainSwallowed_ = true;
                    UpdateHold();
                    return 1;
                }
            }
            else if (isUp)
            {
                const bool swallowed = mainSwallowed_;
                mainDown_            = false;
                mainSwallowed_       = false;
                UpdateHold();
                if (swallowed)
                    return 1;
            }
        }
    }

    return 0;
}

void KeyboardHook::UpdateHold()
{
    const bool satisfied = (mods_ == requiredMods_) && mainDown_;

    if (satisfied && !active_)
    {
        active_ = true;
        MaskWinIfNeeded(requiredMods_);
        if (onActivate_)
            onActivate_();
    }
    else if (!satisfied && active_)
    {
        active_ = false;
        if (onDeactivate_)
            onDeactivate_();
    }
}

void KeyboardHook::MaskWinIfNeeded(unsigned mods)
{
    if ((mods & dk::MOD_WINKEY) && !masked_)
    {
        SendMaskKey();
        masked_ = true;
    }
}
