// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace glass
{
    // A layout or theme installed from a pack whose signature Windows trusted. Kept on this PC,
    // because what a file says about itself proves nothing.
    struct SignedItem
    {
        std::wstring FilePath{};

        std::wstring SignerName{};
        std::wstring IssuerName{};
        std::wstring Thumbprint{};

        // From a trusted timestamp, as a FILETIME, or 0.
        int64_t SignedAt{ 0 };

        struct InstalledFile
        {
            std::wstring Path{};
            uint64_t Size{ 0 };
            std::wstring Sha256{};
        };

        // Every file the install wrote, as it wrote them.
        std::vector<InstalledFile> Files{};
    };

    // Measures the files now, so call it straight after the install. Replaces any earlier entry
    // for the same item.
    void RememberSignedItem(
        _In_ SignedItem const& item,
        _In_ std::vector<std::wstring> const& installedFiles) noexcept;

    void ForgetSignedItem(_In_ std::wstring const& filePath) noexcept;

    // Only while every file is still exactly as it was installed. Changed on this PC, it is no
    // longer what was signed.
    std::optional<SignedItem> SignedItemFor(_In_ std::wstring const& filePath) noexcept;

    // Where the list is kept. Tests point it at a folder of their own.
    void UseSignedItemsFile(_In_ std::wstring const& path) noexcept;
}
