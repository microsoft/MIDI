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
        // Files the first builds wrote as ".midipatch.json" are renamed first, because Explorer
        // sees that as a JSON file and never offers this app for it.
        bool LoadAll(_Out_ std::vector<PatchDocument>& patches) noexcept;

        // Brings a patch file from anywhere into the patch folder under a name of its own, and
        // returns it as read from there. Nothing is written unless the file reads as a patch. A
        // file that is already in the folder is returned as it is.
        std::optional<PatchDocument> Import(_In_ std::wstring const& sourcePath) noexcept;

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

        static constexpr wchar_t FileExtension[] = L".midipatch";
        static constexpr wchar_t LegacyFileExtension[] = L".midipatch.json";

        // A patch file under either name.
        static bool IsPatchFileName(_In_ std::wstring_view fileName) noexcept;

    private:
        PatchStore() noexcept;

        std::optional<PatchDocument> LoadFile(_In_ std::wstring const& path) noexcept;

        // Copies a file an earlier version wrote into the "Earlier versions" folder, under a
        // name no other copy has, and records where. False leaves the original as the only copy,
        // so the caller must not rewrite it.
        bool KeepEarlierVersion(_Inout_ PatchDocument& patch) noexcept;

        // Gives every old ".midipatch.json" in the folder the new extension, unless a file
        // already has that name.
        void RenameLegacyFiles() noexcept;

        std::wstring m_folder{};
        winrt::hstring m_lastError{};
    };
}
