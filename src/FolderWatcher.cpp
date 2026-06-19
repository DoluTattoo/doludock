// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Dolu <https://github.com/DoluTattoo>

#include "FolderWatcher.h"

FolderWatcher::~FolderWatcher()
{
    Stop();
}

void FolderWatcher::Start(const std::wstring& path, HWND notify, UINT msg)
{
    Stop();
    if (path.empty() || !notify)
        return;

    // Open the directory for change monitoring. The permissive share mode keeps
    // the folder fully usable by Explorer and everyone else while we watch it.
    HANDLE dir = CreateFileW(path.c_str(), FILE_LIST_DIRECTORY,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                             OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                             nullptr);
    if (dir == INVALID_HANDLE_VALUE)
        return;

    HANDLE stop = CreateEventW(nullptr, TRUE, FALSE, nullptr); // manual-reset
    if (!stop)
    {
        CloseHandle(dir);
        return;
    }

    dir_       = dir;
    stopEvent_ = stop;
    thread_    = std::thread(&FolderWatcher::Run, dir, stop, notify, msg);
}

void FolderWatcher::Stop()
{
    if (stopEvent_)
        SetEvent(stopEvent_);
    if (thread_.joinable())
        thread_.join();
    if (dir_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(dir_);
        dir_ = INVALID_HANDLE_VALUE;
    }
    if (stopEvent_)
    {
        CloseHandle(stopEvent_);
        stopEvent_ = nullptr;
    }
}

void FolderWatcher::Run(HANDLE dir, HANDLE stopEvent, HWND notify, UINT msg)
{
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr); // manual-reset
    if (!ov.hEvent)
        return;

    std::vector<BYTE> buffer(64 * 1024); // allocator-provided storage is DWORD-aligned
    const DWORD       filter =
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES;

    HANDLE waits[2] = { stopEvent, ov.hEvent };
    for (;;)
    {
        ResetEvent(ov.hEvent);
        DWORD bytes = 0;
        if (!ReadDirectoryChangesW(dir, buffer.data(), static_cast<DWORD>(buffer.size()), FALSE,
                                   filter, &bytes, &ov, nullptr))
            break;

        const DWORD wr = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
        if (wr != WAIT_OBJECT_0 + 1) // stop requested (or the wait failed)
        {
            CancelIo(dir);
            DWORD ignored = 0;
            GetOverlappedResult(dir, &ov, &ignored, TRUE); // let the cancel settle
            break;
        }

        DWORD transferred = 0;
        if (!GetOverlappedResult(dir, &ov, &transferred, TRUE))
            break;

        // Something changed (transferred == 0 means the buffer overflowed, which
        // still warrants a refresh). Bursts are coalesced on the UI thread.
        PostMessageW(notify, msg, 0, 0);
    }

    CloseHandle(ov.hEvent);
}
