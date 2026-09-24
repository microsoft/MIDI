// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include "JsonText.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace glass
{
    namespace
    {
        namespace mjson = winrt::Windows::Data::Json;

        constexpr wchar_t IndentUnit[] = L"  ";
    }

    _Use_decl_annotations_
    std::wstring QuoteJsonString(std::wstring_view value) noexcept
    {
        try
        {
            return std::wstring{ mjson::JsonValue::CreateStringValue(winrt::hstring{ value }).Stringify() };
        }
        catch (...)
        {
            return L"\"\"";
        }
    }

    _Use_decl_annotations_
    std::wstring FormatJsonNumber(double value) noexcept
    {
        if (!std::isfinite(value))
        {
            return L"0";
        }

        // A whole number written as "48.0" and read back as 48 would rewrite itself on the next
        // save, which is exactly the churn this writer exists to avoid.
        if (value == std::floor(value) && std::fabs(value) < 9.0e15)
        {
            return std::format(L"{}", static_cast<int64_t>(value));
        }

        return std::format(L"{}", value);
    }

    void JsonTextWriter::Separate() noexcept
    {
        if (m_needsComma)
        {
            m_text += L",";
        }

        if (!m_text.empty())
        {
            m_text += L"\n";
        }

        Indent();
        m_needsComma = true;
    }

    void JsonTextWriter::Indent() noexcept
    {
        for (int32_t i = 0; i < m_depth; ++i)
        {
            m_text += IndentUnit;
        }
    }

    void JsonTextWriter::BeginObject() noexcept
    {
        Separate();
        m_text += L"{";
        ++m_depth;
        m_needsComma = false;
    }

    _Use_decl_annotations_
    void JsonTextWriter::BeginObject(std::wstring_view key) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": {";
        ++m_depth;
        m_needsComma = false;
    }

    void JsonTextWriter::EndObject() noexcept
    {
        --m_depth;
        m_text += L"\n";
        Indent();
        m_text += L"}";
        m_needsComma = true;
    }

    _Use_decl_annotations_
    void JsonTextWriter::BeginArray(std::wstring_view key) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": [";
        ++m_depth;
        m_needsComma = false;
    }

    void JsonTextWriter::EndArray() noexcept
    {
        --m_depth;
        m_text += L"\n";
        Indent();
        m_text += L"]";
        m_needsComma = true;
    }

    _Use_decl_annotations_
    void JsonTextWriter::Write(std::wstring_view key, std::wstring_view value) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": ";
        m_text += QuoteJsonString(value);
    }

    _Use_decl_annotations_
    void JsonTextWriter::Write(std::wstring_view key, int64_t value) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": ";
        m_text += std::format(L"{}", value);
    }

    _Use_decl_annotations_
    void JsonTextWriter::Write(std::wstring_view key, double value) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": ";
        m_text += FormatJsonNumber(value);
    }

    _Use_decl_annotations_
    void JsonTextWriter::Write(std::wstring_view key, bool value) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += value ? L": true" : L": false";
    }

    _Use_decl_annotations_
    void JsonTextWriter::WriteRaw(std::wstring_view key, std::wstring_view json) noexcept
    {
        Separate();
        m_text += QuoteJsonString(key);
        m_text += L": ";
        m_text += json;
    }

    _Use_decl_annotations_
    void JsonTextWriter::WriteArrayValue(int64_t value) noexcept
    {
        Separate();
        m_text += std::format(L"{}", value);
    }

    _Use_decl_annotations_
    std::wstring CanonicalJson(mjson::IJsonValue const& value, int32_t indentDepth) noexcept
    {
        try
        {
            if (value == nullptr)
            {
                return L"null";
            }

            std::wstring indent{};
            std::wstring inner{};

            for (int32_t i = 0; i < indentDepth; ++i)
            {
                indent += IndentUnit;
            }

            inner = indent + IndentUnit;

            switch (value.ValueType())
            {
            case mjson::JsonValueType::Object:
            {
                auto const object = value.GetObject();

                std::vector<std::wstring> keys{};

                for (auto const& pair : object)
                {
                    keys.push_back(std::wstring{ pair.Key() });
                }

                if (keys.empty())
                {
                    return L"{}";
                }

                std::sort(keys.begin(), keys.end());

                std::wstring text{ L"{" };

                for (size_t i = 0; i < keys.size(); ++i)
                {
                    if (i > 0)
                    {
                        text += L",";
                    }

                    text += L"\n";
                    text += inner;
                    text += QuoteJsonString(keys[i]);
                    text += L": ";
                    text += CanonicalJson(object.Lookup(winrt::hstring{ keys[i] }), indentDepth + 1);
                }

                text += L"\n";
                text += indent;
                text += L"}";

                return text;
            }

            case mjson::JsonValueType::Array:
            {
                auto const array = value.GetArray();

                if (array.Size() == 0)
                {
                    return L"[]";
                }

                std::wstring text{ L"[" };

                for (uint32_t i = 0; i < array.Size(); ++i)
                {
                    if (i > 0)
                    {
                        text += L",";
                    }

                    text += L"\n";
                    text += inner;
                    text += CanonicalJson(array.GetAt(i), indentDepth + 1);
                }

                text += L"\n";
                text += indent;
                text += L"]";

                return text;
            }

            case mjson::JsonValueType::String:
                return QuoteJsonString(std::wstring{ value.GetString() });

            case mjson::JsonValueType::Number:
                return FormatJsonNumber(value.GetNumber());

            case mjson::JsonValueType::Boolean:
                return value.GetBoolean() ? L"true" : L"false";

            case mjson::JsonValueType::Null:
            default:
                return L"null";
            }
        }
        catch (...)
        {
            return L"null";
        }
    }

    _Use_decl_annotations_
    std::vector<std::wstring> UnknownKeys(
        mjson::JsonObject const& object,
        std::vector<std::wstring_view> const& knownKeys) noexcept
    {
        std::vector<std::wstring> keys{};

        try
        {
            if (object == nullptr)
            {
                return keys;
            }

            for (auto const& pair : object)
            {
                std::wstring const key{ pair.Key() };

                auto const known = std::any_of(knownKeys.begin(), knownKeys.end(),
                    [&key](std::wstring_view candidate) { return candidate == key; });

                if (!known)
                {
                    keys.push_back(key);
                }
            }

            std::sort(keys.begin(), keys.end());
        }
        catch (...)
        {
        }

        return keys;
    }

    _Use_decl_annotations_
    mjson::JsonObject CaptureUnknown(
        mjson::JsonObject const& object,
        std::vector<std::wstring_view> const& knownKeys) noexcept
    {
        try
        {
            auto const keys = UnknownKeys(object, knownKeys);

            if (keys.empty())
            {
                return nullptr;
            }

            mjson::JsonObject captured{};

            for (auto const& key : keys)
            {
                captured.Insert(winrt::hstring{ key }, object.Lookup(winrt::hstring{ key }));
            }

            return captured;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    _Use_decl_annotations_
    void WriteUnknown(JsonTextWriter& writer, mjson::JsonObject const& unknown) noexcept
    {
        try
        {
            if (unknown == nullptr)
            {
                return;
            }

            auto const keys = UnknownKeys(unknown, {});

            for (auto const& key : keys)
            {
                writer.WriteRaw(key, CanonicalJson(unknown.Lookup(winrt::hstring{ key }), writer.Depth()));
            }
        }
        catch (...)
        {
        }
    }
}
