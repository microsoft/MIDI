// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "CommandLineOptions.h"

namespace midiplayer
{
    inline constexpr wchar_t InstanceKey[] = L"midiplayer";

    // Double clicking a second file starts a second process. That process finds the running one,
    // posts the paths to it and exits, so there is still only one player and the file is not lost.
    //
    // WM_COPYDATA carries the paths as one buffer of null separated strings. It is synchronous and
    // marshals the buffer across the process boundary, which is exactly what is needed here and
    // avoids inventing a pipe or a named object for a handful of file names.
    inline constexpr ULONG_PTR OpenFilesMessageId = 0x4D494449;   // 'MIDI'

    // Returns false when there was nothing to send or the other window had gone.
    bool SendFilesToExistingInstance(_In_ std::vector<std::wstring> const& paths) noexcept;

    // Reads the paths back out of a WM_COPYDATA received by the player's window. Everything in
    // the buffer is treated as untrusted: the sender is another process.
    std::vector<std::wstring> ReadFilesFromCopyData(_In_ COPYDATASTRUCT const* const data) noexcept;
}
