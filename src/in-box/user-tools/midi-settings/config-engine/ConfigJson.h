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

#pragma once

// JSON helpers shared by the engine. Windows.Data.Json throws on a missing key or a value of
// the wrong type, and nothing read from a package can be trusted to have either, so every read
// here hands back an empty value instead.

#include "ConfigEngine.h"

#include <string>
#include <string_view>

namespace midisettings::configengine::jsonutil
{
    // UTF-8, with or without a byte order mark. Null when it isn't a JSON object.
    json::JsonObject ParseObject(_In_ std::string_view utf8) noexcept;

    // Indented the way the API writes the file, so a configuration stays readable by hand.
    std::string Serialize(_In_ json::JsonObject const& document) noexcept;

    std::wstring ToWide(_In_ std::string_view utf8);
    std::string ToUtf8(_In_ std::wstring_view text);

    std::wstring StringValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept;
    json::JsonObject ObjectValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept;
    json::JsonArray ArrayValue(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept;

    // The object under the key, added when it isn't there or isn't an object.
    json::JsonObject EnsureObject(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept;

    // The key as written, found without regard to case. Empty when there's no such key.
    std::wstring FindKey(_In_ json::JsonObject const& object, _In_ std::wstring_view key) noexcept;

    json::JsonObject CopyObject(_In_ json::JsonObject const& object) noexcept;

    // The same text for two values that say the same thing, whatever order their keys are in
    // and whatever case their strings use. The service compares IDs without regard to case.
    std::wstring IdentityText(_In_ json::IJsonValue const& value) noexcept;

    // {xxxxxxxx-xxxx-...}, in the case the caller asks for
    std::wstring NewGuidText(_In_ bool uppercase) noexcept;

    // The GUID's text without braces, for building the IDs the API builds from one.
    std::wstring GuidTextWithoutBraces(_In_ std::wstring_view guidText);
}
