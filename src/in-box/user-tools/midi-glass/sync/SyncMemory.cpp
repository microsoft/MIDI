// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Before windows.h, so the JSON projection is declared without the GetObject macro.
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "SyncMemory.h"
#include "SyncFileIndex.h"

#include "ContentPack.h"
#include "DeviceLinkFrames.h"

#include <windows.h>

#undef GetObject

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>

namespace glass::sync
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyRecords[] = L"records";
        constexpr wchar_t KeyLayout[] = L"layout";
        constexpr wchar_t KeySender[] = L"sender";
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeySha256[] = L"sha256";

        constexpr size_t MaximumRecords = 512;
        constexpr uintmax_t MaximumFileBytes = 1024 * 1024;

        // One writer at a time in this process.
        std::mutex g_recordsLock{};

        std::wstring Lower(_In_ std::wstring value)
        {
            std::transform(value.begin(), value.end(), value.begin(),
                [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });

            return value;
        }

        std::wstring StringOf(_In_ mjson::JsonObject const& value, _In_ wchar_t const* key)
        {
            if (!value.HasKey(key) || value.GetNamedValue(key).ValueType() != mjson::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetNamedString(key) };
        }

        bool SamePath(_In_ std::wstring const& first, _In_ std::wstring const& second) noexcept
        {
            return ::CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
        }
    }

    _Use_decl_annotations_
    std::wstring SyncLayoutKey(std::wstring const& layoutId, std::wstring const& primaryFileName)
    {
        return layoutId.empty() ? L"file:" + Lower(primaryFileName) : L"id:" + Lower(layoutId);
    }

    // ------------------------------------------------------------------------ approvals

    SessionApprovals& SessionApprovals::Current() noexcept
    {
        // Never destroyed, so nothing can use it after it's gone during shutdown.
        static SessionApprovals* approvals = new SessionApprovals{};
        return *approvals;
    }

    _Use_decl_annotations_
    bool SessionApprovals::IsApproved(std::wstring const& layoutKey, std::wstring const& senderKey) const noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };
            return m_approved.contains({ layoutKey, Lower(senderKey) });
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    void SessionApprovals::Approve(std::wstring const& layoutKey, std::wstring const& senderKey) noexcept
    {
        try
        {
            if (layoutKey.empty() || senderKey.empty())
            {
                return;
            }

            std::scoped_lock guard{ m_lock };
            m_approved.insert({ layoutKey, Lower(senderKey) });
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void SessionApprovals::ForgetSender(std::wstring const& senderKey) noexcept
    {
        try
        {
            auto const sender = Lower(senderKey);

            std::scoped_lock guard{ m_lock };
            std::erase_if(m_approved, [&sender](auto const& entry) { return entry.second == sender; });
        }
        catch (...)
        {
        }
    }

    void SessionApprovals::Clear() noexcept
    {
        try
        {
            std::scoped_lock guard{ m_lock };
            m_approved.clear();
        }
        catch (...)
        {
        }
    }

    // ------------------------------------------------------------------------ records

    std::vector<SyncRecord> SyncRecords::Load() const
    {
        std::vector<SyncRecord> records{};

        std::error_code ec{};
        auto const size = std::filesystem::file_size(m_filePath, ec);

        if (ec || size == 0 || size > MaximumFileBytes)
        {
            return records;
        }

        std::ifstream file{ std::filesystem::path{ m_filePath }, std::ios::binary };
        std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

        bool valid{ false };
        auto const text = midiapp::DeviceLinkFromUtf8(
            std::span<uint8_t const>{ reinterpret_cast<uint8_t const*>(bytes.data()), bytes.size() }, valid);

        mjson::JsonObject root{ nullptr };

        if (!valid || !mjson::JsonObject::TryParse(text, root) || root == nullptr ||
            !root.HasKey(KeyRecords) || root.GetNamedValue(KeyRecords).ValueType() != mjson::JsonValueType::Array)
        {
            return records;
        }

        for (auto const& item : root.GetNamedArray(KeyRecords))
        {
            if (records.size() >= MaximumRecords)
            {
                break;
            }

            if (item.ValueType() != mjson::JsonValueType::Object)
            {
                continue;
            }

            auto const value = item.GetObject();

            SyncRecord record{};
            record.LayoutKey = StringOf(value, KeyLayout);
            record.SenderKey = Lower(StringOf(value, KeySender));
            record.InstalledPath = StringOf(value, KeyPath);
            record.InstalledSha256 = StringOf(value, KeySha256);

            if (!record.LayoutKey.empty() && !record.SenderKey.empty() && !record.InstalledPath.empty() && !record.InstalledSha256.empty())
            {
                records.push_back(std::move(record));
            }
        }

        return records;
    }

    _Use_decl_annotations_
    bool SyncRecords::Save(std::vector<SyncRecord> const& records) const noexcept
    {
        try
        {
            mjson::JsonArray list{};

            for (auto const& record : records)
            {
                mjson::JsonObject value{};
                value.SetNamedValue(KeyLayout, mjson::JsonValue::CreateStringValue(record.LayoutKey));
                value.SetNamedValue(KeySender, mjson::JsonValue::CreateStringValue(record.SenderKey));
                value.SetNamedValue(KeyPath, mjson::JsonValue::CreateStringValue(record.InstalledPath));
                value.SetNamedValue(KeySha256, mjson::JsonValue::CreateStringValue(record.InstalledSha256));

                list.Append(value);
            }

            mjson::JsonObject root{};
            root.SetNamedValue(L"formatVersion", mjson::JsonValue::CreateNumberValue(1));
            root.SetNamedValue(KeyRecords, list);

            auto const utf8 = midiapp::DeviceLinkUtf8(root.Stringify());

            std::filesystem::path const path{ m_filePath };

            std::error_code ec{};
            std::filesystem::create_directories(path.parent_path(), ec);

            auto temporary = path;
            temporary += L".writing";

            {
                std::ofstream file{ temporary, std::ios::binary | std::ios::trunc };

                if (!file)
                {
                    return false;
                }

                file.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));

                if (!file.good())
                {
                    file.close();
                    std::filesystem::remove(temporary, ec);
                    return false;
                }
            }

            if (!::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
            {
                std::filesystem::remove(temporary, ec);
                return false;
            }

            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::optional<SyncRecord> SyncRecords::Find(std::wstring const& layoutKey, std::wstring const& senderKey) const noexcept
    {
        try
        {
            std::scoped_lock guard{ g_recordsLock };

            auto const sender = Lower(senderKey);

            for (auto const& record : Load())
            {
                if (record.LayoutKey == layoutKey && record.SenderKey == sender)
                {
                    return record;
                }
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool SyncRecords::Remember(SyncRecord const& record) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_recordsLock };

            auto records = Load();

            auto copy = record;
            copy.SenderKey = Lower(copy.SenderKey);

            std::erase_if(records, [&copy](SyncRecord const& r) { return r.LayoutKey == copy.LayoutKey && r.SenderKey == copy.SenderKey; });

            records.insert(records.begin(), std::move(copy));

            if (records.size() > MaximumRecords)
            {
                records.resize(MaximumRecords);
            }

            return Save(records);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    bool SyncRecords::ForgetSender(std::wstring const& senderKey) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_recordsLock };

            auto records = Load();
            auto const sender = Lower(senderKey);

            if (std::erase_if(records, [&sender](SyncRecord const& r) { return r.SenderKey == sender; }) == 0)
            {
                return true;
            }

            return Save(records);
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    LocalCopyState CheckLocalCopy(
        SyncRecords const& records,
        std::wstring const& layoutKey,
        std::wstring const& senderKey,
        std::wstring const& existingPath) noexcept
    {
        try
        {
            auto const record = records.Find(layoutKey, senderKey);

            if (!record || existingPath.empty())
            {
                return LocalCopyState::Unknown;
            }

            if (!SamePath(record->InstalledPath, existingPath))
            {
                return LocalCopyState::ChangedHere;
            }

            auto const now = HashFileSha256(existingPath, midiapp::MaximumContentPackBytes);

            return now == record->InstalledSha256 ? LocalCopyState::Unchanged : LocalCopyState::ChangedHere;
        }
        catch (...)
        {
            return LocalCopyState::Unknown;
        }
    }
}
