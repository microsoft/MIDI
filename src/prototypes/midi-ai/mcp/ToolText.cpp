// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "ToolText.h"

#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>

namespace midimcp
{
    _Use_decl_annotations_
    std::wstring Utf8ToWide(std::string_view text)
    {
        if (text.empty())
        {
            return {};
        }

        auto const required = ::MultiByteToWideChar(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);

        if (required <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(required), L'\0');

        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required);

        return result;
    }

    _Use_decl_annotations_
    std::string WideToUtf8(std::wstring_view text)
    {
        if (text.empty())
        {
            return {};
        }

        auto const required = ::WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);

        if (required <= 0)
        {
            return {};
        }

        std::string result(static_cast<size_t>(required), '\0');

        ::WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr);

        return result;
    }

    _Use_decl_annotations_
    void LogLine(std::wstring_view text) noexcept
    {
        try
        {
            auto const bytes = WideToUtf8(L"midi-mcp-spike: " + std::wstring{ text } + L"\n");
            DWORD written{ 0 };
            ::WriteFile(::GetStdHandle(STD_ERROR_HANDLE), bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        }
        catch (...)
        {
        }
    }

    namespace
    {
        json::IJsonValue ValueOrNull(json::JsonObject const& parent, std::wstring_view key) noexcept
        {
            try
            {
                if (parent != nullptr && parent.HasKey(key))
                {
                    return parent.GetNamedValue(key);
                }
            }
            catch (...)
            {
            }

            return nullptr;
        }
    }

    _Use_decl_annotations_
    json::JsonObject ObjectOrNull(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        try
        {
            auto const value = ValueOrNull(parent, key);

            if (value != nullptr && value.ValueType() == json::JsonValueType::Object)
            {
                return value.GetObject();
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    json::JsonArray ArrayOrNull(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        try
        {
            auto const value = ValueOrNull(parent, key);

            if (value != nullptr && value.ValueType() == json::JsonValueType::Array)
            {
                return value.GetArray();
            }
        }
        catch (...)
        {
        }

        return nullptr;
    }

    _Use_decl_annotations_
    std::wstring StringOrEmpty(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        try
        {
            auto const value = ValueOrNull(parent, key);

            if (value != nullptr && value.ValueType() == json::JsonValueType::String)
            {
                return std::wstring{ value.GetString() };
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    std::optional<bool> OptionalBool(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        try
        {
            auto const value = ValueOrNull(parent, key);

            if (value != nullptr && value.ValueType() == json::JsonValueType::Boolean)
            {
                return value.GetBoolean();
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::optional<int64_t> IntegerFromValue(json::IJsonValue const& value) noexcept
    {
        try
        {
            if (value == nullptr)
            {
                return std::nullopt;
            }

            if (value.ValueType() == json::JsonValueType::Number)
            {
                auto const number = value.GetNumber();

                if (std::isfinite(number) && std::floor(number) == number &&
                    std::fabs(number) < 9007199254740992.0)
                {
                    return static_cast<int64_t>(number);
                }

                return std::nullopt;
            }

            if (value.ValueType() == json::JsonValueType::String)
            {
                auto const text = std::wstring{ value.GetString() };

                if (text.empty() || text.size() > 18)
                {
                    return std::nullopt;
                }

                size_t used{ 0 };
                auto const parsed = std::stoll(text, &used, 10);

                if (used == text.size())
                {
                    return parsed;
                }
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::optional<int64_t> OptionalInteger(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        return IntegerFromValue(ValueOrNull(parent, key));
    }

    _Use_decl_annotations_
    std::optional<double> OptionalNumber(json::JsonObject const& parent, std::wstring_view key) noexcept
    {
        try
        {
            auto const value = ValueOrNull(parent, key);

            if (value != nullptr && value.ValueType() == json::JsonValueType::Number &&
                std::isfinite(value.GetNumber()))
            {
                return value.GetNumber();
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    bool EqualsIgnoringCase(std::wstring_view left, std::wstring_view right) noexcept
    {
        return ::CompareStringOrdinal(
            left.data(), static_cast<int>(left.size()),
            right.data(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
    }

    _Use_decl_annotations_
    bool ContainsIgnoringCase(std::wstring_view text, std::wstring_view part) noexcept
    {
        if (part.empty() || part.size() > text.size())
        {
            return false;
        }

        for (size_t i = 0; i + part.size() <= text.size(); i++)
        {
            if (EqualsIgnoringCase(text.substr(i, part.size()), part))
            {
                return true;
            }
        }

        return false;
    }

    _Use_decl_annotations_
    std::wstring Join(std::vector<std::wstring> const& parts, std::wstring_view separator)
    {
        std::wstring text{};

        for (auto const& part : parts)
        {
            if (!text.empty())
            {
                text += separator;
            }

            text += part;
        }

        return text;
    }

    std::wstring NewGuidText() noexcept
    {
        GUID value{};

        if (FAILED(::CoCreateGuid(&value)))
        {
            return {};
        }

        wchar_t buffer[40]{};

        if (::StringFromGUID2(value, buffer, ARRAYSIZE(buffer)) == 0)
        {
            return {};
        }

        // No braces, the same as the ids MIDI Patchbay writes.
        std::wstring result{ buffer };
        std::erase_if(result, [](wchar_t c) { return c == L'{' || c == L'}'; });

        return result;
    }

    int64_t CurrentUnixSeconds() noexcept
    {
        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);

        ULARGE_INTEGER value{};
        value.LowPart = now.dwLowDateTime;
        value.HighPart = now.dwHighDateTime;

        constexpr uint64_t HundredNanosecondsPerSecond = 10000000ull;
        constexpr uint64_t SecondsFrom1601To1970 = 11644473600ull;

        return static_cast<int64_t>(value.QuadPart / HundredNanosecondsPerSecond) -
            static_cast<int64_t>(SecondsFrom1601To1970);
    }

    _Use_decl_annotations_
    std::wstring Base64(std::vector<uint8_t> const& bytes)
    {
        namespace crypto = winrt::Windows::Security::Cryptography;

        auto const buffer = crypto::CryptographicBuffer::CreateFromByteArray(
            winrt::array_view<uint8_t const>{ bytes.data(), static_cast<uint32_t>(bytes.size()) });

        return std::wstring{ crypto::CryptographicBuffer::EncodeToBase64String(buffer) };
    }

    _Use_decl_annotations_
    void Problems::AppendTo(std::wstring& text) const
    {
        if (!Errors.empty())
        {
            text += L"\nProblems that stop this draft:\n";

            for (auto const& error : Errors)
            {
                text += L"- " + error + L"\n";
            }
        }

        if (!Warnings.empty())
        {
            text += L"\nWorth telling the customer:\n";

            for (auto const& warning : Warnings)
            {
                text += L"- " + warning + L"\n";
            }
        }
    }

    namespace
    {
        std::array<std::wstring, 128> const& NoteNames()
        {
            static std::array<std::wstring, 128> const names = []
                {
                    std::array<std::wstring, 128> table{};

                    for (uint8_t note = 0; note < 128; note++)
                    {
                        try
                        {
                            auto const name = midi2msg::MidiMessageHelper::GetNoteDisplayNameFromNoteIndex(note);
                            auto const octave = midi2msg::MidiMessageHelper::GetNoteOctaveFromNoteIndex(note);

                            table[note] = std::wstring{ name } + std::to_wstring(octave);
                        }
                        catch (...)
                        {
                            table[note].clear();
                        }
                    }

                    // Without the SDK there is no house convention to follow, so fall back to the
                    // scientific one rather than invent a third.
                    if (std::any_of(table.begin(), table.end(), [](auto const& n) { return n.empty(); }))
                    {
                        LogLine(L"the MIDI SDK note name helper is not available, so note names use C4 = 60");

                        static constexpr wchar_t const* letters[] =
                            { L"C", L"C#", L"D", L"D#", L"E", L"F", L"F#", L"G", L"G#", L"A", L"A#", L"B" };

                        for (int note = 0; note < 128; note++)
                        {
                            table[note] = std::wstring{ letters[note % 12] } + std::to_wstring(note / 12 - 1);
                        }
                    }

                    return table;
                }();

            return names;
        }
    }

    _Use_decl_annotations_
    std::wstring NoteName(uint8_t note)
    {
        return note < 128 ? NoteNames()[note] : std::to_wstring(note);
    }

    _Use_decl_annotations_
    std::optional<uint8_t> ParseNote(json::IJsonValue const& value) noexcept
    {
        try
        {
            if (value == nullptr)
            {
                return std::nullopt;
            }

            if (value.ValueType() == json::JsonValueType::Number)
            {
                auto const number = IntegerFromValue(value);

                if (number && *number >= 0 && *number <= 127)
                {
                    return static_cast<uint8_t>(*number);
                }

                return std::nullopt;
            }

            if (value.ValueType() != json::JsonValueType::String)
            {
                return std::nullopt;
            }

            std::wstring text{ value.GetString() };
            std::erase_if(text, [](wchar_t c) { return c == L' '; });

            if (auto const number = IntegerFromValue(json::JsonValue::CreateStringValue(text)))
            {
                return *number >= 0 && *number <= 127 ? std::optional<uint8_t>{ static_cast<uint8_t>(*number) } : std::nullopt;
            }

            // Flats are written the way people write them; the tools name notes with sharps.
            static constexpr std::pair<wchar_t const*, wchar_t const*> flats[] =
            {
                { L"DB", L"C#" }, { L"EB", L"D#" }, { L"GB", L"F#" }, { L"AB", L"G#" }, { L"BB", L"A#" },
            };

            std::wstring upper{ text };
            std::transform(upper.begin(), upper.end(), upper.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });

            for (auto const& [flat, sharp] : flats)
            {
                if (upper.rfind(flat, 0) == 0)
                {
                    upper = std::wstring{ sharp } + upper.substr(2);
                    break;
                }
            }

            auto const& names = NoteNames();

            for (uint8_t note = 0; note < 128; note++)
            {
                if (EqualsIgnoringCase(names[note], upper))
                {
                    return note;
                }
            }
        }
        catch (...)
        {
        }

        return std::nullopt;
    }

    _Use_decl_annotations_
    std::wstring DisplayPathUnderDocuments(std::wstring const& fullPath)
    {
        wil::unique_cotaskmem_string documents{};

        if (SUCCEEDED(::SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents)) && documents)
        {
            std::wstring root{ documents.get() };

            if (fullPath.size() > root.size() && EqualsIgnoringCase(std::wstring_view{ fullPath }.substr(0, root.size()), root))
            {
                return L"Documents" + fullPath.substr(root.size());
            }
        }

        return std::filesystem::path{ fullPath }.filename().wstring();
    }
}
