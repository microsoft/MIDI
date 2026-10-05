// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midisoundfontsynth
{
    // The shell's own open dialog, because the WinRT picker needs a window handle workaround
    // and still cannot open a file from every place the customer keeps banks. Empty when the
    // customer cancels. UI thread.
    std::wstring PickSoundFontFile(
        _In_ HWND owner,
        _In_ std::wstring const& startFolder,
        _In_ std::wstring const& title,
        _In_ std::wstring const& filterName,
        _In_ std::wstring const& allFilesName) noexcept;
}
