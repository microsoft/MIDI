// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "CalcHistory.h"

namespace json = ::winrt::Windows::Data::Json;

namespace midicalculator
{
    namespace
    {
        // A history file this size is far past 500 entries; anything bigger isn't ours to read.
        constexpr uint64_t MaximumFileBytes = 4 * 1024 * 1024;

        std::filesystem::path HistoryFolder()
        {
            wil::unique_cotaskmem_string localAppData{};

            THROW_IF_FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData));

            return std::filesystem::path{ localAppData.get() } / L"Microsoft" / L"MIDI" / L"Calculator";
        }

        std::wstring StringOrEmpty(_In_ json::JsonObject const& item, _In_ wchar_t const* name)
        {
            if (item.HasKey(name) && item.GetNamedValue(name).ValueType() == json::JsonValueType::String)
            {
                return std::wstring{ item.GetNamedString(name) };
            }

            return {};
        }
    }

    History& History::Current() noexcept
    {
        static History instance{};
        return instance;
    }

    void History::Load() noexcept
    {
        try
        {
            m_entries.clear();

            auto const file = HistoryFolder() / L"history.json";
            std::error_code error{};

            if (!std::filesystem::exists(file, error) || std::filesystem::file_size(file, error) > MaximumFileBytes || error)
            {
                return;
            }

            wil::unique_hfile handle{ ::CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) };

            if (!handle)
            {
                return;
            }

            LARGE_INTEGER size{};
            THROW_IF_WIN32_BOOL_FALSE(::GetFileSizeEx(handle.get(), &size));

            std::string bytes(static_cast<size_t>(size.QuadPart), '\0');
            DWORD read{ 0 };

            if (!bytes.empty())
            {
                THROW_IF_WIN32_BOOL_FALSE(::ReadFile(handle.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr));
            }

            bytes.resize(read);

            json::JsonObject root{ nullptr };

            if (!json::JsonObject::TryParse(winrt::to_hstring(bytes), root) ||
                !root.HasKey(L"entries") ||
                root.GetNamedValue(L"entries").ValueType() != json::JsonValueType::Array)
            {
                return;
            }

            for (auto const& value : root.GetNamedArray(L"entries"))
            {
                if (value.ValueType() != json::JsonValueType::Object)
                {
                    continue;
                }

                auto const item = value.GetObject();
                HistoryEntry entry{};
                entry.Page = StringOrEmpty(item, L"page");
                entry.Question = StringOrEmpty(item, L"question");
                entry.Answer = StringOrEmpty(item, L"answer");

                if (item.HasKey(L"time") && item.GetNamedValue(L"time").ValueType() == json::JsonValueType::Number)
                {
                    entry.FileTime = static_cast<int64_t>(item.GetNamedNumber(L"time"));
                }

                if (item.HasKey(L"pinned") && item.GetNamedValue(L"pinned").ValueType() == json::JsonValueType::Boolean)
                {
                    entry.Pinned = item.GetNamedBoolean(L"pinned");
                }

                if (entry.Answer.empty())
                {
                    continue;
                }

                m_entries.push_back(std::move(entry));
            }

            Trim();
        }
        CATCH_LOG()
    }

    void History::Save() noexcept
    {
        try
        {
            json::JsonArray entries{};

            for (auto const& entry : m_entries)
            {
                json::JsonObject item{};
                item.SetNamedValue(L"page", json::JsonValue::CreateStringValue(entry.Page));
                item.SetNamedValue(L"question", json::JsonValue::CreateStringValue(entry.Question));
                item.SetNamedValue(L"answer", json::JsonValue::CreateStringValue(entry.Answer));
                // a double holds a FILETIME to within a microsecond or so, which is plenty here
                item.SetNamedValue(L"time", json::JsonValue::CreateNumberValue(static_cast<double>(entry.FileTime)));
                item.SetNamedValue(L"pinned", json::JsonValue::CreateBooleanValue(entry.Pinned));
                entries.Append(item);
            }

            json::JsonObject root{};
            root.SetNamedValue(L"entries", entries);

            auto const folder = HistoryFolder();
            std::filesystem::create_directories(folder);

            auto const bytes = winrt::to_string(root.Stringify());
            auto const temporary = folder / L"history.json.tmp";

            {
                wil::unique_hfile handle{ ::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) };
                THROW_LAST_ERROR_IF(!handle);

                DWORD written{ 0 };
                THROW_IF_WIN32_BOOL_FALSE(::WriteFile(handle.get(), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr));
            }

            // replaced in one step, so a crash mid-write leaves the old history rather than half a file
            THROW_IF_WIN32_BOOL_FALSE(::MoveFileExW(temporary.c_str(), (folder / L"history.json").c_str(), MOVEFILE_REPLACE_EXISTING));
        }
        CATCH_LOG()
    }

    _Use_decl_annotations_
    void History::Add(std::wstring const& page, std::wstring const& question, std::wstring const& answer) noexcept
    {
        try
        {
            if (answer.empty())
            {
                return;
            }

            FILETIME now{};
            ::GetSystemTimePreciseAsFileTime(&now);

            HistoryEntry entry{ page, question, answer,
                static_cast<int64_t>((static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime), false };

            auto const same = std::find_if(m_entries.begin(), m_entries.end(), [&](HistoryEntry const& existing)
                {
                    return existing.Page == page && existing.Question == question && existing.Answer == answer;
                });

            if (same != m_entries.end())
            {
                entry.Pinned = same->Pinned;
                m_entries.erase(same);
            }

            m_entries.insert(m_entries.begin(), std::move(entry));

            Trim();
            Save();
            Notify();
        }
        CATCH_LOG()
    }

    _Use_decl_annotations_
    void History::TogglePin(size_t index) noexcept
    {
        if (index >= m_entries.size())
        {
            return;
        }

        m_entries[index].Pinned = !m_entries[index].Pinned;

        Trim();
        Save();
        Notify();
    }

    _Use_decl_annotations_
    void History::Remove(size_t index) noexcept
    {
        if (index >= m_entries.size())
        {
            return;
        }

        m_entries.erase(m_entries.begin() + static_cast<std::ptrdiff_t>(index));

        Save();
        Notify();
    }

    void History::Clear() noexcept
    {
        std::erase_if(m_entries, [](HistoryEntry const& entry) { return !entry.Pinned; });

        Save();
        Notify();
    }

    _Use_decl_annotations_
    void History::Changed(std::function<void()> const& handler) noexcept
    {
        try
        {
            m_changed = handler;
        }
        CATCH_LOG()
    }

    void History::Trim() noexcept
    {
        size_t unpinned{ 0 };

        std::erase_if(m_entries, [&](HistoryEntry const& entry)
            {
                if (entry.Pinned)
                {
                    return false;
                }

                return ++unpinned > MaximumEntries;
            });
    }

    void History::Notify() noexcept
    {
        try
        {
            if (m_changed)
            {
                m_changed();
            }
        }
        CATCH_LOG()
    }
}
