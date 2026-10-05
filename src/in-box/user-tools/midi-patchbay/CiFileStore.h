// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "CapabilityInquiry.h"

#include <map>

namespace midipatchbay
{
    // A MIDI-CI responder's file, as last read.
    struct CiFileState
    {
        // There is a file by that name in the patch folder.
        bool Found{ false };

        // Null when there is no file, or it isn't a JSON object.
        std::shared_ptr<CiDescription const> Description{};

        std::vector<CiFileProblem> Problems{};
    };

    // MIDI-CI files live beside the patches, in the patch folder, by name only, and are read again
    // whenever one changes. Only the UI thread uses this.
    class CiFileStore
    {
    public:
        static CiFileStore& Current() noexcept;

        // As last read, unless the file has changed since.
        CiFileState Read(_In_ std::wstring const& fileName) noexcept;

        // True when a file read before has changed, appeared or gone since.
        bool CheckForChanges() noexcept;

        // Empty for a name that isn't a plain file name.
        std::wstring PathOf(_In_ std::wstring const& fileName) const;

    private:
        CiFileStore() noexcept = default;

        struct Stamp
        {
            bool Exists{ false };
            uint64_t WriteTime{ 0 };
            uint64_t Size{ 0 };

            bool operator==(Stamp const&) const = default;
        };

        struct Entry
        {
            Stamp Seen{};
            CiFileState State{};
        };

        static Stamp StampOf(_In_ std::wstring const& path) noexcept;
        static CiFileState Load(_In_ std::wstring const& path, _In_ Stamp const& stamp) noexcept;

        // By lowercased file name.
        std::map<std::wstring, Entry> m_entries{};
    };
}
