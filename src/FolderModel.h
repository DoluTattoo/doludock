// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#pragma once

#include "Common.h"
#include <wincodec.h>

#include <cstdint>

// One entry (file or folder) shown inside the overlay.
struct DockEntry
{
    std::wstring path;
    std::wstring name;
    bool         isFolder = false;
    ComPtr<IWICBitmapSource> icon; // device-independent 32bpp PBGRA icon/thumbnail
};

// Raw, COM-free pixels for one icon, produced on a worker thread and handed to
// the UI thread (which turns them back into a WIC bitmap). Keeping the cross-
// thread payload as plain bytes avoids any COM apartment / lifetime concerns.
struct IconPixels
{
    UINT              width  = 0;
    UINT              height = 0;
    std::vector<BYTE> bgra; // 32bpp premultiplied BGRA, stride = width * 4
};

// A batch of freshly-loaded icons, tagged with the folder generation they were
// requested for so stale results (folder changed meanwhile) can be discarded.
struct IconBatch
{
    uint64_t                generation = 0;
    std::vector<IconPixels> icons; // index-aligned with FolderModel::Items()
};

// Enumerates the contents of a folder and loads an icon for each entry.
class FolderModel
{
public:
    bool SetFolder(const std::wstring& path);

    // Enumerates the folder entries (names only, fast). Icons are loaded
    // separately and asynchronously via StartIconLoad(). Bumps the generation.
    void Refresh();

    // Spawns a detached worker that loads all icons off the UI thread and posts
    // `msg` (with an IconBatch* in lParam) to `notify` when done.
    void StartIconLoad(HWND notify, UINT msg, int pixelSize);

    // UI-thread: installs a finished batch into the model if it is still current
    // (its generation matches). Returns true if the icons were applied.
    bool ApplyIcons(IWICImagingFactory* wic, const IconBatch& batch);

    uint64_t Generation() const { return generation_; }

    const std::vector<DockEntry>& Items() const { return items_; }
    const std::wstring&           FolderPath() const { return folderPath_; }
    std::wstring                  FolderName() const;

private:
    std::wstring           folderPath_;
    std::vector<DockEntry> items_;
    uint64_t               generation_ = 0;
};
