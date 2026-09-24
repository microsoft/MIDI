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
#include <vector>

#include "LayoutSerializer.h"

namespace glass
{
    // One .midilayout.json per layout, in the customer's Documents folder, exactly the shape
    // MIDI Patchbay uses for patches. This is the only part of the document layer that touches
    // a disk.
    constexpr wchar_t LayoutFolderName[] = L"MIDI Layouts";
    constexpr wchar_t LayoutFileExtension[] = L".midilayout.json";

    // Created if it is not there. Empty when it cannot be.
    std::wstring LayoutsFolder() noexcept;

    // Reads and parses. A layout from outside the layouts folder is marked imported, which is
    // what makes the app ask before it sends system exclusive to a device on that layout's
    // behalf: a layout file is data, but a sequence step can hold arbitrary SysEx, and arbitrary
    // SysEx sent to the wrong device can do real damage to it.
    ReadResult ReadLayoutFile(_In_ std::wstring const& filePath) noexcept;

    // Writes the canonical form. Returns false without touching the file on any failure.
    bool WriteLayoutFile(_In_ LayoutDocument const& document, _In_ std::wstring const& filePath) noexcept;

    // Every layout in the layouts folder, by full path, sorted.
    std::vector<std::wstring> ListLayoutFiles() noexcept;

    // True when this path is inside the layouts folder.
    bool IsInLayoutsFolder(_In_ std::wstring const& filePath) noexcept;
}
