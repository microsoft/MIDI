// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
//
// UNSUPPORTED - DO NOT COPY THIS APPROACH. This code reads and writes the Windows MIDI Services
// configuration file directly. Microsoft does not support direct manipulation of that file by
// anything other than the in-box MIDI tools. Its format, its name, its location and the registry
// value that selects it are implementation details and can change in any release without notice,
// so this code can corrupt a configuration or stop working at any time. It exists for development
// and diagnostics inside the Windows MIDI Services repository only. Applications and third-party
// tools must use the Windows MIDI Services API (Windows.Devices.Midi2.ServiceConfig) instead.
// ============================================================================

// Pure: no precompiled header, so the unit tests compile this file exactly as it ships.

#include "ConfigJson.h"

#include <combaseapi.h>

#include <algorithm>
#include <cwctype>
#include <vector>

namespace midisettings::configengine
{
    _Use_decl_annotations_
    bool SameText(std::wstring_view const left, std::wstring_view const right) noexcept
    {
        if (left.size() != right.size())
        {
            return false;
        }

        if (left.empty())
        {
            return true;
        }

        return ::CompareStringOrdinal(
            left.data(), static_cast<int>(left.size()),
            right.data(), static_cast<int>(right.size()),
            TRUE) == CSTR_EQUAL;
    }

    _Use_decl_annotations_
    bool EndsWithText(std::wstring_view const text, std::wstring_view const suffix) noexcept
    {
        return text.size() >= suffix.size() &&
            SameText(text.substr(text.size() - suffix.size()), suffix);
    }

    namespace
    {
        // Same length as the input for everything the invariant locale lowercases, which is what
        // lets a position found in the copy be used in the original.
        std::wstring InvariantLowercase(_In_ std::wstring_view const text)
        {
            std::wstring result(text.size(), L'\0');

            if (!text.empty() &&
                ::LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                    text.data(), static_cast<int>(text.size()),
                    result.data(), static_cast<int>(result.size()),
                    nullptr, nullptr, 0) != static_cast<int>(text.size()))
            {
                return std::wstring{ text };
            }

            return result;
        }
    }

    _Use_decl_annotations_
    bool ContainsText(std::wstring_view const text, std::wstring_view const part) noexcept
    {
        try
        {
            if (part.empty() || part.size() > text.size())
            {
                return false;
            }

            return InvariantLowercase(text).find(InvariantLowercase(part)) != std::wstring::npos;
        }
        catch (...)
        {
            return false;
        }
    }

    _Use_decl_annotations_
    std::wstring ReplaceText(std::wstring const& text, std::wstring_view const part, std::wstring_view const replacement)
    {
        if (part.empty() || text.empty())
        {
            return text;
        }

        auto const lowerText = InvariantLowercase(text);
        auto const lowerPart = InvariantLowercase(part);

        std::wstring result{};
        size_t start{ 0 };

        for (;;)
        {
            auto const found = lowerText.find(lowerPart, start);

            if (found == std::wstring::npos)
            {
                result.append(text, start, std::wstring::npos);
                break;
            }

            result.append(text, start, found - start);
            result.append(replacement);
            start = found + part.size();
        }

        return result;
    }

    _Use_decl_annotations_
    bool IsUsableFileName(std::wstring_view const name) noexcept
    {
        if (name.empty() || name.size() >= MAX_PATH)
        {
            return false;
        }

        if (name.find_first_not_of(L'.') == std::wstring_view::npos)
        {
            return false;
        }

        // a trailing space or period is quietly dropped by Windows, so the name would change
        if (name.back() == L' ' || name.back() == L'.')
        {
            return false;
        }

        for (auto const ch : name)
        {
            if (ch < 0x20 || ch == L'\\' || ch == L'/' || ch == L':' || ch == L'*' ||
                ch == L'?' || ch == L'"' || ch == L'<' || ch == L'>' || ch == L'|')
            {
                return false;
            }
        }

        return true;
    }

    _Use_decl_annotations_
    std::wstring PortNamingName(PortNaming const naming)
    {
        switch (naming)
        {
        case PortNaming::ClassicCompatible: return L"classicCompatible";
        case PortNaming::NewStyle:          return L"newStyle";
        case PortNaming::Automatic:         return L"automatic";
        default:                            return {};
        }
    }

    _Use_decl_annotations_
    PortNaming PortNamingFromName(std::wstring_view const name) noexcept
    {
        if (SameText(name, L"classicCompatible"))
        {
            return PortNaming::ClassicCompatible;
        }

        if (SameText(name, L"newStyle"))
        {
            return PortNaming::NewStyle;
        }

        if (SameText(name, L"automatic"))
        {
            return PortNaming::Automatic;
        }

        return PortNaming::None;
    }

    _Use_decl_annotations_
    std::wstring FileTimeToUtcText(FILETIME const& time)
    {
        SYSTEMTIME utc{};

        if (!::FileTimeToSystemTime(&time, &utc))
        {
            return {};
        }

        wchar_t text[32]{};

        if (::swprintf_s(text, L"%04u-%02u-%02uT%02u:%02u:%02uZ",
            static_cast<unsigned>(utc.wYear), static_cast<unsigned>(utc.wMonth), static_cast<unsigned>(utc.wDay),
            static_cast<unsigned>(utc.wHour), static_cast<unsigned>(utc.wMinute), static_cast<unsigned>(utc.wSecond)) < 0)
        {
            return {};
        }

        return std::wstring{ text };
    }

    _Use_decl_annotations_
    bool TryParseUtcText(std::wstring_view const text, FILETIME& time) noexcept
    {
        time = {};

        try
        {
            // 2026-10-14T19:20:31Z, exactly
            if (text.size() != 20 || text[4] != L'-' || text[7] != L'-' || text[10] != L'T' ||
                text[13] != L':' || text[16] != L':' || (text[19] != L'Z' && text[19] != L'z'))
            {
                return false;
            }

            auto const number = [&text](size_t const start, size_t const count, WORD& value)
                {
                    uint32_t result{ 0 };

                    for (size_t i = start; i < start + count; i++)
                    {
                        if (text[i] < L'0' || text[i] > L'9')
                        {
                            return false;
                        }

                        result = result * 10 + static_cast<uint32_t>(text[i] - L'0');
                    }

                    value = static_cast<WORD>(result);
                    return true;
                };

            SYSTEMTIME utc{};

            if (!number(0, 4, utc.wYear) || !number(5, 2, utc.wMonth) || !number(8, 2, utc.wDay) ||
                !number(11, 2, utc.wHour) || !number(14, 2, utc.wMinute) || !number(17, 2, utc.wSecond))
            {
                return false;
            }

            return ::SystemTimeToFileTime(&utc, &time) != FALSE;
        }
        catch (...)
        {
            time = {};
            return false;
        }
    }

    FILETIME CurrentFileTime() noexcept
    {
        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);

        return now;
    }

    _Use_decl_annotations_
    uint64_t FileTimeValue(FILETIME const& time) noexcept
    {
        return (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    }
}

namespace midisettings::configengine::jsonutil
{
    namespace
    {
        constexpr std::wstring_view CommentKey{ L"_comment" };
        constexpr size_t IndentSpaces{ 4 };

        void AppendIndent(_Inout_ std::wstring& text, _In_ int32_t const depth)
        {
            text.append(static_cast<size_t>(depth) * IndentSpaces, L' ');
        }

        // The same layout the API writes: four spaces, and the comment first in each object.
        void AppendPretty(
            _Inout_ std::wstring& text,
            _In_ json::IJsonValue const& value,
            _In_ int32_t const depth)
        {
            if (value == nullptr)
            {
                text += L"null";
                return;
            }

            switch (value.ValueType())
            {
            case json::JsonValueType::Object:
            {
                auto const object = value.GetObject();

                if (object.Size() == 0)
                {
                    text += L"{}";
                    return;
                }

                text += L"{\n";

                uint32_t index{ 0 };
                auto const size = object.Size();

                auto const appendMember = [&](winrt::hstring const& key, json::IJsonValue const& member)
                    {
                        AppendIndent(text, depth + 1);

                        text += json::JsonValue::CreateStringValue(key).Stringify();
                        text += L": ";

                        AppendPretty(text, member, depth + 1);

                        if (++index < size)
                        {
                            text += L",";
                        }

                        text += L"\n";
                    };

                winrt::hstring const commentKey{ CommentKey };
                auto const hasComment = object.HasKey(commentKey);

                if (hasComment)
                {
                    appendMember(commentKey, object.GetNamedValue(commentKey));
                }

                for (auto const& pair : object)
                {
                    if (hasComment && pair.Key() == commentKey)
                    {
                        continue;
                    }

                    appendMember(pair.Key(), pair.Value());
                }

                AppendIndent(text, depth);
                text += L"}";
                return;
            }

            case json::JsonValueType::Array:
            {
                auto const array = value.GetArray();

                if (array.Size() == 0)
                {
                    text += L"[]";
                    return;
                }

                text += L"[\n";

                for (uint32_t i = 0; i < array.Size(); i++)
                {
                    AppendIndent(text, depth + 1);
                    AppendPretty(text, array.GetAt(i), depth + 1);

                    if (i + 1 < array.Size())
                    {
                        text += L",";
                    }

                    text += L"\n";
                }

                AppendIndent(text, depth);
                text += L"]";
                return;
            }

            default:
                text += value.Stringify();
                return;
            }
        }

        std::wstring Lowercase(_In_ std::wstring_view text)
        {
            std::wstring result{ text };

            if (!result.empty())
            {
                ::LCMapStringEx(
                    LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE,
                    text.data(), static_cast<int>(text.size()),
                    result.data(), static_cast<int>(result.size()),
                    nullptr, nullptr, 0);
            }

            return result;
        }

        void AppendIdentity(_Inout_ std::wstring& text, _In_ json::IJsonValue const& value)
        {
            if (value == nullptr)
            {
                text += L"null";
                return;
            }

            switch (value.ValueType())
            {
            case json::JsonValueType::Object:
            {
                std::vector<std::pair<std::wstring, json::IJsonValue>> members{};

                for (auto const& pair : value.GetObject())
                {
                    if (SameText(pair.Key(), CommentKey))
                    {
                        continue;
                    }

                    members.emplace_back(Lowercase(pair.Key()), pair.Value());
                }

                std::sort(members.begin(), members.end(), [](auto const& left, auto const& right)
                    {
                        return left.first < right.first;
                    });

                text += L"{";

                for (auto const& member : members)
                {
                    text += json::JsonValue::CreateStringValue(member.first).Stringify();
                    text += L":";
                    AppendIdentity(text, member.second);
                    text += L",";
                }

                text += L"}";
                return;
            }

            case json::JsonValueType::Array:
            {
                auto const array = value.GetArray();

                text += L"[";

                for (uint32_t i = 0; i < array.Size(); i++)
                {
                    AppendIdentity(text, array.GetAt(i));
                    text += L",";
                }

                text += L"]";
                return;
            }

            case json::JsonValueType::String:
                text += json::JsonValue::CreateStringValue(Lowercase(value.GetString())).Stringify();
                return;

            default:
                text += value.Stringify();
                return;
            }
        }
    }

    _Use_decl_annotations_
    std::wstring ToWide(std::string_view const utf8)
    {
        if (utf8.empty())
        {
            return {};
        }

        auto const required = ::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);

        if (required <= 0)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(required), L'\0');

        if (::MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(), static_cast<int>(utf8.size()), result.data(), required) != required)
        {
            return {};
        }

        return result;
    }

    _Use_decl_annotations_
    std::string ToUtf8(std::wstring_view const text)
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

        if (::WideCharToMultiByte(
            CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr) != required)
        {
            return {};
        }

        return result;
    }

    _Use_decl_annotations_
    json::JsonObject ParseObject(std::string_view utf8) noexcept
    {
        try
        {
            if (utf8.size() >= 3 &&
                static_cast<uint8_t>(utf8[0]) == 0xEF &&
                static_cast<uint8_t>(utf8[1]) == 0xBB &&
                static_cast<uint8_t>(utf8[2]) == 0xBF)
            {
                utf8.remove_prefix(3);
            }

            auto const wide = ToWide(utf8);

            if (wide.empty())
            {
                return nullptr;
            }

            json::JsonObject document{ nullptr };

            if (!json::JsonObject::TryParse(winrt::hstring{ wide }, document))
            {
                return nullptr;
            }

            return document;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::string Serialize(json::JsonObject const& document) noexcept
    {
        try
        {
            if (document == nullptr)
            {
                return {};
            }

            std::wstring text{};
            AppendPretty(text, document, 0);

            return ToUtf8(text);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring StringValue(json::JsonObject const& object, std::wstring_view const key) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return {};
            }

            auto const value = object.TryLookup(winrt::hstring{ key });

            if (value == nullptr || value.ValueType() != json::JsonValueType::String)
            {
                return {};
            }

            return std::wstring{ value.GetString() };
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    json::JsonObject ObjectValue(json::JsonObject const& object, std::wstring_view const key) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return nullptr;
            }

            auto const value = object.TryLookup(winrt::hstring{ key });

            if (value == nullptr || value.ValueType() != json::JsonValueType::Object)
            {
                return nullptr;
            }

            return value.GetObject();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    json::JsonArray ArrayValue(json::JsonObject const& object, std::wstring_view const key) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return nullptr;
            }

            auto const value = object.TryLookup(winrt::hstring{ key });

            if (value == nullptr || value.ValueType() != json::JsonValueType::Array)
            {
                return nullptr;
            }

            return value.GetArray();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    json::JsonObject EnsureObject(json::JsonObject const& object, std::wstring_view const key) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return nullptr;
            }

            auto existing = ObjectValue(object, key);

            if (existing != nullptr)
            {
                return existing;
            }

            json::JsonObject added{};
            object.SetNamedValue(winrt::hstring{ key }, added);

            return added;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::wstring FindKey(json::JsonObject const& object, std::wstring_view const key) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return {};
            }

            for (auto const& pair : object)
            {
                if (SameText(pair.Key(), key))
                {
                    return std::wstring{ pair.Key() };
                }
            }
        }
        catch (...)
        {
        }

        return {};
    }

    _Use_decl_annotations_
    json::JsonObject CopyObject(json::JsonObject const& object) noexcept
    {
        try
        {
            if (object == nullptr)
            {
                return nullptr;
            }

            return json::JsonObject::Parse(object.Stringify());
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    std::wstring IdentityText(json::IJsonValue const& value) noexcept
    {
        try
        {
            std::wstring text{};
            AppendIdentity(text, value);

            return text;
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring NewGuidText(bool const uppercase) noexcept
    {
        try
        {
            GUID id{};

            if (FAILED(::CoCreateGuid(&id)))
            {
                return {};
            }

            wchar_t buffer[40]{};

            if (::StringFromGUID2(id, buffer, ARRAYSIZE(buffer)) == 0)
            {
                return {};
            }

            std::wstring text{ buffer };

            return uppercase ? text : Lowercase(text);
        }
        catch (...)
        {
            return {};
        }
    }

    _Use_decl_annotations_
    std::wstring GuidTextWithoutBraces(std::wstring_view guidText)
    {
        if (!guidText.empty() && guidText.front() == L'{')
        {
            guidText.remove_prefix(1);
        }

        if (!guidText.empty() && guidText.back() == L'}')
        {
            guidText.remove_suffix(1);
        }

        return std::wstring{ guidText };
    }
}
