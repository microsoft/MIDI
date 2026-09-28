// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Builds commands and reads responses the way an app would. The keys are written out here rather
// than taken from the transport's headers, so a renamed key fails a test instead of following it.

namespace RtpMidiTest
{
    namespace json = winrt::Windows::Data::Json;

    inline bool IsSuccess(json::JsonObject const& response)
    {
        return response != nullptr && response.GetNamedBoolean(L"success", false);
    }

    inline uint32_t ErrorCode(json::JsonObject const& response)
    {
        if (response == nullptr || !response.HasKey(L"errorCode")) return 0;
        return static_cast<uint32_t>(response.GetNamedNumber(L"errorCode", 0));
    }

    inline std::wstring Command(std::wstring const& verb, std::map<std::wstring, std::wstring> const& arguments = {})
    {
        json::JsonObject argumentsObject;
        for (auto const& argument : arguments) argumentsObject.SetNamedValue(argument.first, json::JsonValue::CreateStringValue(argument.second));

        json::JsonObject command;
        command.SetNamedValue(L"commandName", json::JsonValue::CreateStringValue(verb));
        command.SetNamedValue(L"commandArguments", argumentsObject);

        json::JsonObject root;
        root.SetNamedValue(L"transportCommand", command);

        return std::wstring{ root.Stringify() };
    }

    inline std::wstring NewGuidText()
    {
        GUID guid{};
        VERIFY_SUCCEEDED(CoCreateGuid(&guid));

        wchar_t buffer[40]{};
        StringFromGUID2(guid, buffer, ARRAYSIZE(buffer));
        return buffer;
    }

    inline json::JsonObject FindEntry(json::JsonObject const& response, std::wstring const& arrayKey, std::wstring const& entryId)
    {
        if (response == nullptr || !response.HasKey(arrayKey)) return nullptr;

        auto const entries = response.GetNamedArray(arrayKey);

        for (uint32_t i = 0; i < entries.Size(); i++)
        {
            auto const entry = entries.GetObjectAt(i);
            if (SameText(std::wstring{ entry.GetNamedString(L"entryIdentifier", L"") }, entryId)) return entry;
        }

        return nullptr;
    }

    inline json::JsonObject FindConnection(json::JsonObject const& entry, std::wstring const& remoteName)
    {
        if (entry == nullptr || !entry.HasKey(L"connections")) return nullptr;

        auto const connections = entry.GetNamedArray(L"connections");

        for (uint32_t i = 0; i < connections.Size(); i++)
        {
            auto const connection = connections.GetObjectAt(i);
            if (std::wstring{ connection.GetNamedString(L"remoteName", L"") } == remoteName) return connection;
        }

        return nullptr;
    }

    inline std::wstring EntryState(json::JsonObject const& entry)
    {
        return entry == nullptr ? std::wstring{} : std::wstring{ entry.GetNamedString(L"entryState", L"") };
    }
}
