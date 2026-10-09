// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CiFileStore.h"
#include "PatchStore.h"
#include "TextMatch.h"

#include <fstream>
#include <iterator>

namespace midipatchbay
{
    CiFileStore& CiFileStore::Current() noexcept
    {
        static CiFileStore instance{};
        return instance;
    }

    _Use_decl_annotations_
    std::wstring CiFileStore::PathOf(std::wstring const& fileName) const
    {
        auto const& folder = PatchStore::Current().FolderPath();

        if (folder.empty() || !IsCiFileName(fileName))
        {
            return {};
        }

        return (std::filesystem::path{ folder } / fileName).wstring();
    }

    _Use_decl_annotations_
    CiFileStore::Stamp CiFileStore::StampOf(std::wstring const& path) noexcept
    {
        Stamp stamp{};

        WIN32_FILE_ATTRIBUTE_DATA data{};

        if (!path.empty() &&
            ::GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) &&
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            stamp.Exists = true;
            stamp.WriteTime = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
            stamp.Size = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        }

        return stamp;
    }

    _Use_decl_annotations_
    CiFileState CiFileStore::Load(std::wstring const& path, Stamp const& stamp) noexcept
    {
        CiFileState state{};

        try
        {
            if (!stamp.Exists)
            {
                return state;
            }

            state.Found = true;

            if (stamp.Size > MaximumCiFileBytes)
            {
                state.Problems.push_back(CiFileProblem{ CiFileProblemKind::TooLarge, -1, {} });
                return state;
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };

            if (!file)
            {
                state.Found = false;
                return state;
            }

            std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            // It may have grown since it was looked at.
            if (bytes.size() > MaximumCiFileBytes)
            {
                state.Problems.push_back(CiFileProblem{ CiFileProblemKind::TooLarge, -1, {} });
                return state;
            }

            // A byte order mark is allowed, and dropped.
            if (bytes.size() >= 3 &&
                static_cast<uint8_t>(bytes[0]) == 0xEF &&
                static_cast<uint8_t>(bytes[1]) == 0xBB &&
                static_cast<uint8_t>(bytes[2]) == 0xBF)
            {
                bytes.erase(0, 3);
            }

            auto const text = winrt::to_hstring(bytes);

            std::vector<CiFileProblem> problems{};

            state.Description = ParseCiDescription(std::wstring_view{ text }, problems);
            state.Problems = std::move(problems);
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to read a MIDI-CI file.")

        return state;
    }

    _Use_decl_annotations_
    CiFileState CiFileStore::Read(std::wstring const& fileName) noexcept
    {
        try
        {
            auto const path = PathOf(fileName);

            if (path.empty())
            {
                return {};
            }

            auto const key = LowerCopy(fileName);
            auto const stamp = StampOf(path);

            auto const found = m_entries.find(key);

            if (found != m_entries.end() && found->second.Seen == stamp)
            {
                return found->second.State;
            }

            Entry entry{};
            entry.Seen = stamp;
            entry.State = Load(path, stamp);

            m_entries.insert_or_assign(key, entry);

            return entry.State;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to look up a MIDI-CI file.")

        return {};
    }

    bool CiFileStore::CheckForChanges() noexcept
    {
        try
        {
            bool changed{ false };

            // Forgotten rather than read here, so a file nothing uses any more is only noticed once.
            for (auto entry = m_entries.begin(); entry != m_entries.end();)
            {
                if (StampOf(PathOf(entry->first)) != entry->second.Seen)
                {
                    entry = m_entries.erase(entry);
                    changed = true;
                }
                else
                {
                    ++entry;
                }
            }

            return changed;
        }
        MIDI_PATCHBAY_CATCH_AND_LOG(L"Unable to check the MIDI-CI files.")

        return false;
    }
}
