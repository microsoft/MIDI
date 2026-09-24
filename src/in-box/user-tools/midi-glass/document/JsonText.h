// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

namespace glass
{
    // Writes JSON with a fixed key order and a fixed shape.
    //
    // Reading is done with the platform parser, because a layout is untrusted input and a
    // hand-written parser is the wrong place to learn that. Writing is done here instead of with
    // JsonObject::Stringify because a JsonObject is a map and does not promise to give the keys
    // back in the order they went in. A file that reorders itself every time it is saved cannot
    // be diffed, cannot be round-trip tested, and looks like a change to source control when
    // nothing was edited.
    class JsonTextWriter
    {
    public:
        void BeginObject() noexcept;
        void BeginObject(_In_ std::wstring_view key) noexcept;
        void EndObject() noexcept;

        void BeginArray(_In_ std::wstring_view key) noexcept;
        void EndArray() noexcept;

        void Write(_In_ std::wstring_view key, _In_ std::wstring_view value) noexcept;
        void Write(_In_ std::wstring_view key, _In_ int64_t value) noexcept;
        void Write(_In_ std::wstring_view key, _In_ double value) noexcept;
        void Write(_In_ std::wstring_view key, _In_ bool value) noexcept;

        // For an already-serialized fragment, which is how the parts of a file this build did
        // not understand are put back.
        void WriteRaw(_In_ std::wstring_view key, _In_ std::wstring_view json) noexcept;

        void WriteArrayValue(_In_ int64_t value) noexcept;
        void WriteArrayString(_In_ std::wstring_view value) noexcept;

        int32_t Depth() const noexcept { return m_depth; }

        bool IsEmpty() const noexcept { return m_text.empty(); }

        std::wstring const& Text() const noexcept { return m_text; }

    private:
        void Separate() noexcept;
        void Indent() noexcept;

        std::wstring m_text{};
        int32_t m_depth{ 0 };
        bool m_needsComma{ false };
    };

    // A JSON string literal, quoted and escaped. Goes through the platform so the escaping rules
    // are the platform's rather than ours.
    std::wstring QuoteJsonString(_In_ std::wstring_view value) noexcept;

    // A number, in the shortest form that reads back as the same value. Whole numbers lose the
    // trailing zero so a coordinate does not drift between "48" and "48.0" across saves.
    std::wstring FormatJsonNumber(_In_ double value) noexcept;

    // Re-serializes a parsed value with its object keys sorted, at the given indent. Used for the
    // parts of a file this build did not understand: we cannot know their shape, but we can make
    // sure writing them out twice produces the same bytes.
    std::wstring CanonicalJson(
        _In_ winrt::Windows::Data::Json::IJsonValue const& value,
        _In_ int32_t indentDepth) noexcept;

    // The keys of an object that this build did not consume, in sorted order.
    std::vector<std::wstring> UnknownKeys(
        _In_ winrt::Windows::Data::Json::JsonObject const& object,
        _In_ std::vector<std::wstring_view> const& knownKeys) noexcept;

    // The subset of an object holding only the keys this build did not consume, or nullptr when
    // there are none. Stored on the model so a save can put them back.
    winrt::Windows::Data::Json::JsonObject CaptureUnknown(
        _In_ winrt::Windows::Data::Json::JsonObject const& object,
        _In_ std::vector<std::wstring_view> const& knownKeys) noexcept;

    // Writes every key of an unknown-field object, sorted, into an object already begun.
    void WriteUnknown(
        _Inout_ JsonTextWriter& writer,
        _In_ winrt::Windows::Data::Json::JsonObject const& unknown) noexcept;
}
