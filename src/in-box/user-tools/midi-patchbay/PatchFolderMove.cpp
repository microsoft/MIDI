// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include <windows.h>

#include "PatchFolderMove.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace midipatchbay
{
    namespace
    {
        bool IsFolder(_In_ DWORD attributes) noexcept
        {
            return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        }

        // A junction or symbolic link points elsewhere. A OneDrive folder is a reparse point too, but not a link.
        bool IsLink(_In_ std::filesystem::path const& path) noexcept
        {
            WIN32_FIND_DATAW data{};

            auto const find = ::FindFirstFileExW(path.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);

            if (find == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            ::FindClose(find);

            return (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                IsReparseTagNameSurrogate(data.dwReserved0) != 0;
        }

        // A link can move as a link, but nothing is taken out of it.
        bool IsPlainFolder(_In_ std::filesystem::path const& path) noexcept
        {
            return IsFolder(::GetFileAttributesW(path.c_str())) && !IsLink(path);
        }

        void RemoveIfEmpty(_In_ std::filesystem::path const& folder)
        {
            std::error_code ec{};

            if (!std::filesystem::is_empty(folder, ec) || ec)
            {
                return;
            }

            auto const attributes = ::GetFileAttributesW(folder.c_str());

            // Explorer and OneDrive often mark a folder read-only, and RemoveDirectory refuses one even empty.
            if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0)
            {
                ::SetFileAttributesW(folder.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
            }

            ::RemoveDirectoryW(folder.c_str());
        }

        void MoveMissingEntries(
            _In_ std::filesystem::path const& from,
            _In_ std::filesystem::path const& to)
        {
            std::error_code ec{};
            std::vector<std::filesystem::path> entries{};

            for (std::filesystem::directory_iterator entry{ from, ec }, end{}; !ec && entry != end; entry.increment(ec))
            {
                entries.push_back(entry->path());
            }

            for (auto const& source : entries)
            {
                auto const target = to / source.filename();

                // No replace flag: a name `to` already has keeps what it has.
                if (::MoveFileExW(source.c_str(), target.c_str(), 0))
                {
                    continue;
                }

                if (IsPlainFolder(source) && IsPlainFolder(target))
                {
                    MoveMissingEntries(source, target);
                    RemoveIfEmpty(source);
                }
            }
        }
    }

    _Use_decl_annotations_
    std::wstring MoveEarlierPatchFolder(
        std::wstring const& previous,
        std::wstring const& current) noexcept
    {
        try
        {
            if (previous.empty())
            {
                return current;
            }

            auto const previousAttributes = ::GetFileAttributesW(previous.c_str());

            if (!IsFolder(previousAttributes))
            {
                return current;
            }

            auto const currentAttributes = ::GetFileAttributesW(current.c_str());

            if (currentAttributes == INVALID_FILE_ATTRIBUTES)
            {
                // Fails while anything has a file in the folder open, however it's shared.
                return ::MoveFileExW(previous.c_str(), current.c_str(), 0) ? current : previous;
            }

            // A file holds the new name, so the earlier folder is still the only one.
            if (!IsFolder(currentAttributes))
            {
                return previous;
            }

            if (IsPlainFolder(previous))
            {
                MoveMissingEntries(previous, current);
                RemoveIfEmpty(previous);
            }

            return current;
        }
        catch (...)
        {
        }

        return current;
    }
}
