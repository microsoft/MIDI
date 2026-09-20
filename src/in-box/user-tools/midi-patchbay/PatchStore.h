// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "PatchModel.h"

namespace midipatchbay
{
    // Patches are one JSON file each, in Documents\MIDI Patchbay.
    //
    // Deliberately NOT the Windows MIDI Services configuration file: the service does not read
    // routing, a canvas full of connections can get large, and a customer should be able to copy
    // a single patch to another machine. If the service ever gains this feature these files get
    // migrated into the configuration through the supported API.
    //
    // Nothing here throws. A failure leaves the file alone and is reported through
    // LastErrorMessage so the window can say so rather than fail silently.
    class PatchStore
    {
    public:
        static PatchStore& Current() noexcept;

        std::wstring const& FolderPath() const noexcept { return m_folder; }
        winrt::hstring LastErrorMessage() const noexcept { return m_lastError; }

        // A folder that is not there yet is not a failure: the app simply starts with no patches.
        bool LoadAll(_Out_ std::vector<PatchDocument>& patches) noexcept;

        // Writes the patch and fills in FilePath and ModifiedFileTime. When the name changed, the
        // file is written under the new name and the old one is removed, so the folder never
        // accumulates a copy per rename.
        bool Save(_Inout_ PatchDocument& patch) noexcept;

        bool Delete(_In_ PatchDocument const& patch) noexcept;

        // Opens the folder in File Explorer, creating it first so the customer never lands on a
        // missing path.
        void ShowFolder() noexcept;

        bool EnsureFolder() noexcept;

        // Turns a patch name into a file name that is safe on this file system, then makes it
        // unique against what is already there.
        std::wstring BuildUniqueFilePath(
            _In_ std::wstring const& patchName,
            _In_ std::wstring const& currentPath) const noexcept;

        static constexpr wchar_t FileExtension[] = L".midipatch.json";

    private:
        PatchStore() noexcept;

        std::optional<PatchDocument> LoadFile(_In_ std::wstring const& path) noexcept;

        std::wstring m_folder{};
        winrt::hstring m_lastError{};
    };
}
