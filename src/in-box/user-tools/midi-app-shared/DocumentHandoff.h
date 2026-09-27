// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace midiapp
{
    // Double-clicking a document while its tool is already open starts a second copy. That copy
    // finds the running one, hands it the paths and exits, so there is still one window and the
    // file is not lost.
    //
    // WM_COPYDATA carries the paths as one buffer of null separated strings. It is synchronous
    // and copies the buffer across the process boundary, which is all a handful of paths needs.
    inline constexpr ULONG_PTR OpenDocumentsMessageId = 0x4D494444;   // 'MIDD'

    // False when there was nothing to send or no running window to send it to.
    bool SendDocumentsToExistingInstance(
        _In_ std::wstring const& appKey,
        _In_ std::vector<std::wstring> const& paths) noexcept;

    // The paths out of a WM_COPYDATA, or none when it is not ours. Everything in it is untrusted:
    // the sender is another process.
    std::vector<std::wstring> ReadDocumentsFromCopyData(_In_opt_ COPYDATASTRUCT const* data) noexcept;
}
