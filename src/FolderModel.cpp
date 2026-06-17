// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#include "FolderModel.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <algorithm>
#include <thread>

namespace
{
// Loads a high-resolution shell icon for `path` and returns it as raw 32bpp
// premultiplied-BGRA pixels (no COM objects survive the call, so the result is
// safe to hand to another thread). Runs on the icon worker thread.
IconPixels LoadIconPixels(IWICImagingFactory* wic, const std::wstring& path, int size)
{
    IconPixels out;

    ComPtr<IShellItemImageFactory> imgFactory;
    if (FAILED(SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&imgFactory))))
        return out;

    SIZE    s{ size, size };
    HBITMAP hbmp = nullptr;
    if (FAILED(imgFactory->GetImage(s, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbmp)) || !hbmp)
        return out;

    ComPtr<IWICBitmap> wbmp;
    HRESULT hr = wic->CreateBitmapFromHBITMAP(hbmp, nullptr, WICBitmapUsePremultipliedAlpha, &wbmp);
    DeleteObject(hbmp);
    if (FAILED(hr))
        return out;

    ComPtr<IWICFormatConverter> conv;
    if (FAILED(wic->CreateFormatConverter(&conv)))
        return out;
    if (FAILED(conv->Initialize(wbmp.Get(), GUID_WICPixelFormat32bppPBGRA,
                                WICBitmapDitherTypeNone, nullptr, 0.0,
                                WICBitmapPaletteTypeMedianCut)))
        return out;

    UINT w = 0, h = 0;
    if (FAILED(conv->GetSize(&w, &h)) || w == 0 || h == 0)
        return out;

    const UINT stride = w * 4;
    out.bgra.resize(static_cast<size_t>(stride) * h);
    WICRect rc{ 0, 0, static_cast<INT>(w), static_cast<INT>(h) };
    if (FAILED(conv->CopyPixels(&rc, stride, static_cast<UINT>(out.bgra.size()), out.bgra.data())))
        return IconPixels{};

    out.width  = w;
    out.height = h;
    return out;
}

// Returns the name exactly as Windows Explorer shows it: hides the ".lnk"
// extension of shortcuts and other known extensions according to the user's
// folder options. Falls back to the raw file name if the shell call fails.
std::wstring ShellDisplayName(const std::wstring& path, const std::wstring& fallback)
{
    SHFILEINFOW sfi{};
    if (SHGetFileInfoW(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_DISPLAYNAME))
        return sfi.szDisplayName;
    return fallback;
}
} // namespace

bool FolderModel::SetFolder(const std::wstring& path)
{
    if (path.empty())
        return false;
    folderPath_ = path;
    return true;
}

std::wstring FolderModel::FolderName() const
{
    if (folderPath_.empty())
        return L"Folder";
    LPCWSTR leaf = PathFindFileNameW(folderPath_.c_str());
    return (leaf && *leaf) ? std::wstring(leaf) : folderPath_;
}

void FolderModel::Refresh()
{
    ++generation_;
    items_.clear();
    if (folderPath_.empty())
        return;

    std::wstring base = folderPath_;
    if (base.empty() || base.back() != L'\\')
        base += L'\\';

    WIN32_FIND_DATAW fd{};
    HANDLE           h = FindFirstFileW((base + L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    do
    {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;

        const DWORD attr = fd.dwFileAttributes;
        if (attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))
            continue;

        DockEntry it;
        it.path     = base + fd.cFileName;
        it.name     = ShellDisplayName(it.path, fd.cFileName);
        it.isFolder = (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
        items_.push_back(std::move(it));
    } while (FindNextFileW(h, &fd));
    FindClose(h);

    std::sort(items_.begin(), items_.end(), [](const DockEntry& a, const DockEntry& b) {
        if (a.isFolder != b.isFolder)
            return a.isFolder > b.isFolder; // folders first
        return StrCmpLogicalW(a.name.c_str(), b.name.c_str()) < 0;
    });

    constexpr size_t kMaxItems = 96;
    if (items_.size() > kMaxItems)
        items_.resize(kMaxItems);
}

void FolderModel::StartIconLoad(HWND notify, UINT msg, int pixelSize)
{
    const uint64_t gen = generation_;

    std::vector<std::wstring> paths;
    paths.reserve(items_.size());
    for (const auto& it : items_)
        paths.push_back(it.path);

    std::thread([gen, paths = std::move(paths), notify, msg, pixelSize]() mutable {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        auto batch        = std::make_unique<IconBatch>();
        batch->generation = gen;
        {
            ComPtr<IWICImagingFactory> wic;
            if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(&wic))))
            {
                batch->icons.resize(paths.size());
                for (size_t i = 0; i < paths.size(); ++i)
                    batch->icons[i] = LoadIconPixels(wic.Get(), paths[i], pixelSize);
            }
        } // release the WIC factory before CoUninitialize
        CoUninitialize();

        IconBatch* raw = batch.release();
        if (!PostMessageW(notify, msg, 0, reinterpret_cast<LPARAM>(raw)))
            delete raw; // the target window is gone; don't leak the batch
    }).detach();
}

bool FolderModel::ApplyIcons(IWICImagingFactory* wic, const IconBatch& batch)
{
    if (!wic || batch.generation != generation_)
        return false;

    const size_t n = std::min(items_.size(), batch.icons.size());
    for (size_t i = 0; i < n; ++i)
    {
        const IconPixels& px = batch.icons[i];
        if (px.width == 0 || px.height == 0 || px.bgra.empty())
            continue;

        ComPtr<IWICBitmap> bmp;
        const UINT         stride = px.width * 4;
        if (SUCCEEDED(wic->CreateBitmapFromMemory(px.width, px.height,
                                                  GUID_WICPixelFormat32bppPBGRA, stride,
                                                  static_cast<UINT>(px.bgra.size()),
                                                  const_cast<BYTE*>(px.bgra.data()), &bmp)))
            items_[i].icon = bmp;
    }
    return true;
}
