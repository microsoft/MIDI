// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "SysExReview.h"
#include "ContentPack.h"

#include <windows.h>
#include <shlobj_core.h>

// windows.h defines this as GetObjectW, which turns IJsonValue::GetObject() into a compile error.
#undef GetObject

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t KeyItems[] = L"items";
        constexpr wchar_t KeyPath[] = L"path";
        constexpr wchar_t KeyReview[] = L"review";
        constexpr wchar_t KeyApproved[] = L"approved";

        constexpr size_t MaximumItems = 1024;
        constexpr uintmax_t MaximumFileBytes = 2 * 1024 * 1024;

        std::mutex g_lock{};
        std::wstring g_overridePath{};

        struct Item
        {
            std::wstring Path{};
            bool Review{ false };

            // The fingerprint of the SysEx the person said yes to, or empty.
            std::wstring Approved{};
        };

        std::wstring ListPath()
        {
            if (!g_overridePath.empty())
            {
                return g_overridePath;
            }

            PWSTR raw{ nullptr };

            if (FAILED(::SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &raw)))
            {
                return {};
            }

            std::filesystem::path folder{ raw };
            ::CoTaskMemFree(raw);

            folder /= L"Microsoft";
            folder /= L"MIDI Glass";

            std::error_code ignored{};
            std::filesystem::create_directories(folder, ignored);

            return (folder / L"SysExReview.json").wstring();
        }

        bool SamePath(_In_ std::wstring const& first, _In_ std::wstring const& second) noexcept
        {
            return ::CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
        }

        std::wstring StringOf(_In_ mjson::JsonObject const& value, _In_ wchar_t const* key)
        {
            if (!value.HasKey(key) || value.GetNamedValue(key).ValueType() != mjson::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetNamedString(key) };
        }

        std::vector<Item> LoadLocked()
        {
            std::vector<Item> items{};

            auto const path = ListPath();

            std::error_code ec{};
            auto const size = path.empty() ? 0 : std::filesystem::file_size(path, ec);

            if (path.empty() || ec || size == 0 || size > MaximumFileBytes)
            {
                return items;
            }

            std::ifstream file{ std::filesystem::path{ path }, std::ios::binary };
            std::string bytes{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

            auto const length = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);

            if (length <= 0)
            {
                return items;
            }

            std::wstring text(static_cast<size_t>(length), L'\0');
            ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), text.data(), length);

            mjson::JsonObject root{ nullptr };

            if (!mjson::JsonObject::TryParse(text, root) || root == nullptr ||
                !root.HasKey(KeyItems) || root.GetNamedValue(KeyItems).ValueType() != mjson::JsonValueType::Array)
            {
                return items;
            }

            for (auto const& entry : root.GetNamedArray(KeyItems))
            {
                if (items.size() >= MaximumItems)
                {
                    break;
                }

                if (entry.ValueType() != mjson::JsonValueType::Object)
                {
                    continue;
                }

                auto const value = entry.GetObject();

                Item item{};
                item.Path = StringOf(value, KeyPath);
                item.Approved = StringOf(value, KeyApproved);
                item.Review = value.HasKey(KeyReview) && value.GetNamedValue(KeyReview).ValueType() == mjson::JsonValueType::Boolean &&
                    value.GetNamedBoolean(KeyReview);

                if (!item.Path.empty() && (item.Review || !item.Approved.empty()))
                {
                    items.push_back(std::move(item));
                }
            }

            return items;
        }

        bool SaveLocked(_In_ std::vector<Item> const& items) noexcept
        {
            try
            {
                auto const path = ListPath();

                if (path.empty())
                {
                    return false;
                }

                mjson::JsonArray list{};

                for (auto const& item : items)
                {
                    if (!item.Review && item.Approved.empty())
                    {
                        continue;
                    }

                    mjson::JsonObject value{};
                    value.SetNamedValue(KeyPath, mjson::JsonValue::CreateStringValue(item.Path));
                    value.SetNamedValue(KeyReview, mjson::JsonValue::CreateBooleanValue(item.Review));
                    value.SetNamedValue(KeyApproved, mjson::JsonValue::CreateStringValue(item.Approved));

                    list.Append(value);
                }

                mjson::JsonObject root{};
                root.SetNamedValue(KeyItems, list);

                auto const text = std::wstring{ root.Stringify() };
                auto const length = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

                std::string utf8(static_cast<size_t>(std::max(length, 0)), '\0');

                if (length > 0)
                {
                    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), utf8.data(), length, nullptr, nullptr);
                }

                std::filesystem::path target{ path };
                auto temporary = target;
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
                        std::error_code ignored{};
                        std::filesystem::remove(temporary, ignored);
                        return false;
                    }
                }

                if (!::MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING))
                {
                    std::error_code ignored{};
                    std::filesystem::remove(temporary, ignored);
                    return false;
                }

                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        Item* FindLocked(_Inout_ std::vector<Item>& items, _In_ std::wstring const& filePath)
        {
            auto const found = std::find_if(items.begin(), items.end(), [&filePath](Item const& item) { return SamePath(item.Path, filePath); });
            return found == items.end() ? nullptr : &*found;
        }

        // A row that sends SysEx, and the words or bytes it sends.
        bool SendsSystemExclusive(_In_ ControlMessage const& message) noexcept
        {
            switch (message.Kind)
            {
            case MessageKind::SystemExclusive:
                return !message.SystemExclusive.empty();
            case MessageKind::RawUmp:
                return ContainsSystemExclusive(message.RawWords.data(), static_cast<uint32_t>(message.RawWords.size()));
            default:
                return false;
            }
        }

        // Every row that can send SysEx: the controls' own rows, then every sequence's steps.
        template <typename Visit>
        void ForEachSystemExclusiveRow(_In_ LayoutDocument const& document, _In_ Visit const& visit)
        {
            for (auto const& page : document.Pages)
            {
                for (auto const& control : page.Controls)
                {
                    for (auto const& message : control.Messages)
                    {
                        if (SendsSystemExclusive(message))
                        {
                            visit(message);
                        }
                    }
                }
            }

            for (auto const& sequence : document.Sequences)
            {
                for (auto const& step : sequence.Steps)
                {
                    // A step this build doesn't know is never sent.
                    if (!step.UnrecognizedKind.empty())
                    {
                        continue;
                    }

                    if ((step.Kind == SequenceStepKind::SendMidiMessage || step.Kind == SequenceStepKind::SendSystemExclusive) &&
                        SendsSystemExclusive(step.Message))
                    {
                        visit(step.Message);
                    }
                }
            }
        }
    }

    _Use_decl_annotations_
    bool IsSystemExclusivePacket(uint32_t firstWord) noexcept
    {
        auto const type = (firstWord >> 28) & 0x0F;
        return type == 0x3 || type == 0x5;
    }

    _Use_decl_annotations_
    uint32_t UmpWordCount(uint32_t firstWord) noexcept
    {
        switch ((firstWord >> 28) & 0x0F)
        {
        case 0x0: case 0x1: case 0x2: case 0x6: case 0x7:
            return 1;
        case 0x3: case 0x4: case 0x8: case 0x9: case 0xA:
            return 2;
        case 0xB: case 0xC:
            return 3;
        default:
            return 4;
        }
    }

    _Use_decl_annotations_
    std::vector<uint32_t> WithoutSystemExclusive(uint32_t const* words, uint32_t wordCount)
    {
        std::vector<uint32_t> kept{};

        if (words == nullptr)
        {
            return kept;
        }

        uint32_t offset{ 0 };

        while (offset < wordCount)
        {
            auto const size = UmpWordCount(words[offset]);

            if (size > wordCount - offset)
            {
                break;
            }

            if (!IsSystemExclusivePacket(words[offset]))
            {
                kept.insert(kept.end(), words + offset, words + offset + size);
            }

            offset += size;
        }

        return kept;
    }

    _Use_decl_annotations_
    bool ContainsSystemExclusive(uint32_t const* words, uint32_t wordCount) noexcept
    {
        if (words == nullptr)
        {
            return false;
        }

        uint32_t offset{ 0 };

        while (offset < wordCount)
        {
            if (IsSystemExclusivePacket(words[offset]))
            {
                return true;
            }

            offset += UmpWordCount(words[offset]);
        }

        return false;
    }

    _Use_decl_annotations_
    std::vector<std::wstring> SystemExclusiveDevices(LayoutDocument const& document)
    {
        std::vector<std::wstring> devices{};

        ForEachSystemExclusiveRow(document, [&devices](ControlMessage const& message)
            {
                if (std::none_of(devices.begin(), devices.end(), [&message](std::wstring const& name) { return name == message.DeviceName; }))
                {
                    devices.push_back(message.DeviceName);
                }
            });

        return devices;
    }

    _Use_decl_annotations_
    std::wstring SystemExclusiveFingerprint(LayoutDocument const& document)
    {
        std::vector<uint8_t> data{};
        bool any{ false };

        auto const append = [&data](void const* bytes, size_t size)
            {
                auto const begin = static_cast<uint8_t const*>(bytes);
                data.insert(data.end(), begin, begin + size);
            };

        ForEachSystemExclusiveRow(document, [&](ControlMessage const& message)
            {
                any = true;

                // Lengths first, so no two different rows run together into the same bytes.
                auto const nameLength = static_cast<uint32_t>(message.DeviceName.size());
                auto const group = message.GroupIndex;
                auto const kind = static_cast<int32_t>(message.Kind);
                auto const byteCount = static_cast<uint32_t>(message.SystemExclusive.size());
                auto const wordCount = static_cast<uint32_t>(message.RawWords.size());

                append(&nameLength, sizeof(nameLength));
                append(message.DeviceName.data(), message.DeviceName.size() * sizeof(wchar_t));
                append(&group, sizeof(group));
                append(&kind, sizeof(kind));
                append(&byteCount, sizeof(byteCount));
                append(message.SystemExclusive.data(), message.SystemExclusive.size());
                append(&wordCount, sizeof(wordCount));
                append(message.RawWords.data(), message.RawWords.size() * sizeof(uint32_t));
            });

        return any ? midiapp::Sha256Hex(data.data(), data.size()) : std::wstring{};
    }

    _Use_decl_annotations_
    void MarkForSysExReview(std::wstring const& filePath) noexcept
    {
        try
        {
            if (filePath.empty())
            {
                return;
            }

            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();

            if (auto* const item = FindLocked(items, filePath))
            {
                item->Review = true;
            }
            else
            {
                items.push_back(Item{ filePath, true, {} });

                if (items.size() > MaximumItems)
                {
                    items.erase(items.begin());
                }
            }

            SaveLocked(items);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void ForgetSysExReview(std::wstring const& filePath) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();

            if (std::erase_if(items, [&filePath](Item const& item) { return SamePath(item.Path, filePath); }) > 0)
            {
                SaveLocked(items);
            }
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void MoveSysExReview(std::wstring const& fromPath, std::wstring const& toPath) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();
            auto* const item = FindLocked(items, fromPath);

            if (item == nullptr)
            {
                return;
            }

            auto moved = *item;
            moved.Path = toPath;

            std::erase_if(items, [&](Item const& i) { return SamePath(i.Path, fromPath) || SamePath(i.Path, toPath); });
            items.push_back(std::move(moved));

            SaveLocked(items);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void CopySysExReview(std::wstring const& fromPath, std::wstring const& toPath) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();
            auto* const item = FindLocked(items, fromPath);

            if (item == nullptr)
            {
                return;
            }

            auto copy = *item;
            copy.Path = toPath;

            std::erase_if(items, [&toPath](Item const& i) { return SamePath(i.Path, toPath); });
            items.push_back(std::move(copy));

            SaveLocked(items);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    SysExReview CheckSysExReview(LayoutDocument const& document, std::wstring const& filePath) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();
            auto* const item = filePath.empty() ? nullptr : FindLocked(items, filePath);

            auto const marked = document.IsImported || (item != nullptr && item->Review);

            if (!marked)
            {
                return SysExReview::Allowed;
            }

            auto const fingerprint = SystemExclusiveFingerprint(document);
            auto const approved = fingerprint.empty() || (item != nullptr && item->Approved == fingerprint);

            if (!approved)
            {
                return SysExReview::Ask;
            }

            // Nothing to ask about, or already said yes to: the mark has done its job.
            if (item != nullptr && item->Review)
            {
                item->Review = false;
                SaveLocked(items);
            }

            return SysExReview::Allowed;
        }
        catch (...)
        {
            // When the list can't be read, the safe answer is to ask.
            return SysExReview::Ask;
        }
    }

    _Use_decl_annotations_
    void ApproveSysEx(LayoutDocument const& document, std::wstring const& filePath) noexcept
    {
        try
        {
            if (filePath.empty())
            {
                return;
            }

            auto const fingerprint = SystemExclusiveFingerprint(document);

            std::scoped_lock guard{ g_lock };

            auto items = LoadLocked();

            if (auto* const item = FindLocked(items, filePath))
            {
                item->Review = false;
                item->Approved = fingerprint;
            }
            else
            {
                items.push_back(Item{ filePath, false, fingerprint });
            }

            SaveLocked(items);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void UseSysExReviewFile(std::wstring const& path) noexcept
    {
        try
        {
            std::scoped_lock guard{ g_lock };
            g_overridePath = path;
        }
        catch (...)
        {
        }
    }
}
