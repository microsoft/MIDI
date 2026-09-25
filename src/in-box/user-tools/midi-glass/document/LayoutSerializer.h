// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <string>
#include <string_view>

#include "LayoutModel.h"

namespace glass
{
    // Turns a layout into text and back.
    //
    // Reading never throws and never trusts: a layout can arrive from a stranger, so every string
    // is bounded, every collection is capped and anything malformed is dropped rather than
    // believed. A file this build only partly understands still loads, and the parts it did not
    // understand are kept so saving does not destroy them.
    //
    // Writing is deterministic. The same document always produces the same bytes, which is what
    // makes a round trip testable and keeps a saved file from looking edited when it was not.
    struct ReadResult
    {
        bool Succeeded{ false };

        LayoutDocument Document{};

        // Set when the file announced a version this build does not know. The layout still loads;
        // the UI is expected to say plainly that it holds something this version cannot edit.
        bool IsFromNewerVersion{ false };

        // Why it failed, for a log. Not shown to a customer as it stands here.
        std::wstring Detail{};
    };

    ReadResult ReadLayoutFromJson(_In_ std::wstring_view json) noexcept;

    std::wstring WriteLayoutToJson(_In_ LayoutDocument const& document) noexcept;

    // A bare file name, or nothing. A layout is untrusted input, so a name that carries a path
    // is a way to make this app read a file somewhere else on the PC. Shared with the editor so
    // that what gets written can never be something the reader would refuse.
    std::wstring SanitizeFileName(_In_ std::wstring name) noexcept;
}
