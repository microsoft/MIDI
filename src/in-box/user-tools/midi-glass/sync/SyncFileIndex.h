// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The SHA-256 of every file the receiving PC already has in its layouts and themes folders, so
// a sync can ask only for the files it's missing. A file is hashed again only when its size or
// last-write time changes, and the answers are kept in a file under %LOCALAPPDATA%, so a large
// video isn't read every time. A hash found here is only a hint: the pack put together from it
// is checked file by file against the manifest before anything is installed. Pch-free.

#include <sal.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace glass::sync
{
    // Lowercase hex, the way a pack manifest lists it. Empty when the file can't be read or is
    // larger than `maximumBytes`.
    std::wstring HashFileSha256(
        _In_ std::wstring const& path,
        _In_ uint64_t maximumBytes) noexcept;

    class FileHashIndex
    {
    public:
        // An empty path keeps the index in memory only.
        explicit FileHashIndex(_In_ std::wstring cachePath) noexcept;

        // Looks at the files directly inside each folder, hashing new and changed ones, and
        // forgets files that are gone. Reads the cache the first time. With `onlySizes`, a file
        // of any other size isn't hashed, since it can't be one of the files wanted.
        void Refresh(
            _In_ std::vector<std::wstring> const& folders,
            _In_opt_ std::vector<uint64_t> const* onlySizes) noexcept;

        // A file with this hash, or empty.
        std::wstring Find(_In_ std::wstring const& sha256) const noexcept;

        bool Save() const noexcept;

        size_t Count() const noexcept { return m_entries.size(); }

    private:
        struct Entry
        {
            std::wstring Path{};
            uint64_t Size{ 0 };
            int64_t Written{ 0 };
            std::wstring Sha256{};
        };

        void LoadCache() noexcept;

        std::wstring m_cachePath{};
        bool m_loaded{ false };

        // Keyed by the full path in lowercase.
        std::map<std::wstring, Entry> m_entries{};

        // Hash to the key of a file that has it.
        std::map<std::wstring, std::wstring> m_byHash{};
    };
}
