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

        constexpr wchar_t KeyTrusted[] = L"trusted";

        // Each distinct SysEx anyone on this PC wrote or allowed. Far more than a person makes.
        constexpr size_t MaximumItems = 4096;
        constexpr uintmax_t MaximumFileBytes = 2 * 1024 * 1024;

        std::mutex g_lock{};
        std::wstring g_overridePath{};


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

        bool IsFingerprint(_In_ std::wstring const& value) noexcept
        {
            return value.size() == 64 && std::all_of(value.begin(), value.end(),
                [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); });
        }

        // The fingerprints of the SysEx this PC trusts. Empty when the list is missing or damaged,
        // which makes everything with SysEx ask: the safe way to fail.
        std::vector<std::wstring> LoadLocked()
        {
            std::vector<std::wstring> items{};

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
                !root.HasKey(KeyTrusted) || root.GetNamedValue(KeyTrusted).ValueType() != mjson::JsonValueType::Array)
            {
                return items;
            }

            for (auto const& entry : root.GetNamedArray(KeyTrusted))
            {
                if (items.size() >= MaximumItems)
                {
                    break;
                }

                if (entry.ValueType() != mjson::JsonValueType::String)
                {
                    continue;
                }

                auto value = std::wstring{ entry.GetString() };

                if (IsFingerprint(value))
                {
                    items.push_back(std::move(value));
                }
            }

            return items;
        }

        bool SaveLocked(_In_ std::vector<std::wstring> const& items) noexcept
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
                    list.Append(mjson::JsonValue::CreateStringValue(item));
                }

                mjson::JsonObject root{};
                root.SetNamedValue(KeyTrusted, list);

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
    SysExReview CheckSysExReview(LayoutDocument const& document) noexcept
    {
        try
        {
            auto const fingerprint = SystemExclusiveFingerprint(document);

            if (fingerprint.empty())
            {
                return SysExReview::Allowed;
            }

            std::scoped_lock guard{ g_lock };

            auto const trusted = LoadLocked();

            return std::find(trusted.begin(), trusted.end(), fingerprint) != trusted.end()
                ? SysExReview::Allowed
                : SysExReview::Ask;
        }
        catch (...)
        {
            return SysExReview::Ask;
        }
    }

    _Use_decl_annotations_
    void TrustSysEx(LayoutDocument const& document) noexcept
    {
        try
        {
            auto const fingerprint = SystemExclusiveFingerprint(document);

            if (fingerprint.empty())
            {
                return;
            }

            std::scoped_lock guard{ g_lock };

            auto trusted = LoadLocked();

            // Most recent last, so the oldest goes first when the list is full.
            std::erase(trusted, fingerprint);
            trusted.push_back(fingerprint);

            if (trusted.size() > MaximumItems)
            {
                trusted.erase(trusted.begin(), trusted.begin() + static_cast<ptrdiff_t>(trusted.size() - MaximumItems));
            }

            SaveLocked(trusted);
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
