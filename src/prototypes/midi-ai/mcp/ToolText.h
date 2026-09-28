// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// PROTOTYPE. Small helpers shared by the protocol and the tools.

#pragma once

#include "pch.h"

namespace midimcp
{
    std::wstring Utf8ToWide(_In_ std::string_view text);
    std::string WideToUtf8(_In_ std::wstring_view text);

    // One line to stderr, which an MCP host keeps as the server's log. Never stdout: that carries
    // protocol messages and nothing else.
    void LogLine(_In_ std::wstring_view text) noexcept;

    // Reading arguments. Every one of these tolerates a missing key or a value of the wrong type,
    // because the arguments come from a model and a model gets types wrong.
    json::JsonObject ObjectOrNull(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;
    json::JsonArray ArrayOrNull(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;
    std::wstring StringOrEmpty(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;
    std::optional<bool> OptionalBool(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;

    // A whole number, whether the model sent 3 or "3". Anything else is nullopt.
    std::optional<int64_t> OptionalInteger(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;
    std::optional<int64_t> IntegerFromValue(_In_ json::IJsonValue const& value) noexcept;
    std::optional<double> OptionalNumber(_In_ json::JsonObject const& parent, _In_ std::wstring_view key) noexcept;

    bool EqualsIgnoringCase(_In_ std::wstring_view left, _In_ std::wstring_view right) noexcept;
    bool ContainsIgnoringCase(_In_ std::wstring_view text, _In_ std::wstring_view part) noexcept;

    std::wstring Join(_In_ std::vector<std::wstring> const& parts, _In_ std::wstring_view separator);

    std::wstring NewGuidText() noexcept;

    // Seconds since 1970, which is what the tools' own files store.
    int64_t CurrentUnixSeconds() noexcept;

    std::wstring Base64(_In_ std::vector<uint8_t> const& bytes);

    // What went wrong with a request, split by whether it stops the tool. Errors are things the
    // model must fix or ask the customer about; warnings are said once and do not stop anything.
    struct Problems
    {
        std::vector<std::wstring> Errors{};
        std::vector<std::wstring> Warnings{};

        bool HasErrors() const noexcept { return !Errors.empty(); }
        void AppendTo(_Inout_ std::wstring& text) const;
    };

    // Note names exactly as the MIDI tools show them, from the SDK's own helper, so a name the
    // model reads back from a tool is the name the customer sees in MIDI Patchbay.
    std::wstring NoteName(_In_ uint8_t note);

    // Accepts 0 to 127, or a name such as "C4" or "F#2" in the same convention as NoteName.
    std::optional<uint8_t> ParseNote(_In_ json::IJsonValue const& value) noexcept;

    // "Documents\MIDI Patchbay\Studio.midipatch" rather than a full path. A full path carries
    // the customer's account name, and the model has no use for it.
    std::wstring DisplayPathUnderDocuments(_In_ std::wstring const& fullPath);
}
