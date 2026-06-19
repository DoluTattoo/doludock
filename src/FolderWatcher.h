// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#pragma once

#include "Common.h"

#include <thread>

// Watches a single folder for content changes (entries added, removed or
// renamed) on a background thread and posts a message to a window whenever
// something changes, so the overlay can refresh its listing live.
class FolderWatcher
{
public:
    ~FolderWatcher();

    // (Re)starts watching `path`, posting `msg` to `notify` on every change.
    // An empty path (or a folder that can't be opened) just stops watching.
    void Start(const std::wstring& path, HWND notify, UINT msg);

    // Stops watching and joins the worker thread. Safe to call repeatedly.
    void Stop();

private:
    static void Run(HANDLE dir, HANDLE stopEvent, HWND notify, UINT msg);

    std::thread thread_;
    HANDLE      stopEvent_ = nullptr;
    HANDLE      dir_       = INVALID_HANDLE_VALUE;
};
