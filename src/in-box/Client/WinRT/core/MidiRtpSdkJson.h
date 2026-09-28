// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Shared with the transport rather than restated here, so the two cannot drift
#include "..\..\..\Transport\RtpMidiTransport\rtp_json_defs.h"
#include "..\..\..\Transport\RtpMidiTransport\rtp_transport_error_codes.h"

// Fixed by RTP-MIDI itself. Not taken from the transport's transport_defs.h, because its generic
// macro names are also defined by the Network MIDI 2.0 transport header the SDK includes.
#define MIDI_RTP_SDK_DEFAULT_HOST_PORT                                  5004
#define MIDI_RTP_SDK_DNSSD_SERVICE_TYPE                                 L"_apple-midi._udp"
#define MIDI_RTP_SDK_DNSSD_DOMAIN                                       L"local"

// Reads the RTP-MIDI transport's answers. The WinRT JSON default accessors only cover a missing
// key and throw on a wrong type, so every read here checks the type first.
namespace MidiRtpSdkJson
{
    inline json::IJsonValue Find(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key, _In_ json::JsonValueType const type) noexcept
    {
        try
        {
            if (parent == nullptr) return nullptr;

            winrt::hstring const name{ key };
            if (!parent.HasKey(name)) return nullptr;

            auto const value = parent.Lookup(name);
            return value != nullptr && value.ValueType() == type ? value : nullptr;
        }
        catch (...)
        {
            return nullptr;
        }
    }

    inline winrt::hstring String(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::String);
            return value == nullptr ? winrt::hstring{} : value.GetString();
        }
        catch (...)
        {
            return {};
        }
    }

    inline bool Boolean(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key, _In_ bool const defaultValue = false) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Boolean);
            return value == nullptr ? defaultValue : value.GetBoolean();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    // Counts and ports arrive as JSON numbers. Anything negative or out of range is 0.
    template <typename T>
    inline T Unsigned(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Number);
            if (value == nullptr) return T{ 0 };

            auto const number = value.GetNumber();
            if (!(number >= 0) || number > static_cast<double>((std::numeric_limits<T>::max)())) return T{ 0 };

            return static_cast<T>(number);
        }
        catch (...)
        {
            return T{ 0 };
        }
    }

    // an HRESULT is sent as its unsigned 32 bit value
    inline int32_t Hresult(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key) noexcept
    {
        return static_cast<int32_t>(Unsigned<uint32_t>(parent, key));
    }

    inline json::JsonArray Array(_In_ json::JsonObject const& parent, _In_ std::wstring_view const key) noexcept
    {
        try
        {
            auto const value = Find(parent, key, json::JsonValueType::Array);
            return value == nullptr ? json::JsonArray{ nullptr } : value.GetArray();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    // the objects in an array, skipping anything else
    inline std::vector<json::JsonObject> Objects(_In_ json::JsonArray const& array) noexcept
    {
        std::vector<json::JsonObject> objects;

        try
        {
            if (array == nullptr) return objects;

            for (uint32_t i = 0; i < array.Size(); i++)
            {
                auto const value = array.GetAt(i);
                if (value != nullptr && value.ValueType() == json::JsonValueType::Object) objects.push_back(value.GetObject());
            }
        }
        catch (...)
        {
        }

        return objects;
    }

    inline std::vector<winrt::hstring> Strings(_In_ json::JsonArray const& array) noexcept
    {
        std::vector<winrt::hstring> strings;

        try
        {
            if (array == nullptr) return strings;

            for (uint32_t i = 0; i < array.Size(); i++)
            {
                auto const value = array.GetAt(i);
                if (value != nullptr && value.ValueType() == json::JsonValueType::String) strings.push_back(value.GetString());
            }
        }
        catch (...)
        {
        }

        return strings;
    }

    // the transport reports braced uppercase GUIDs, but any form winrt::guid reads is accepted
    inline bool TryGuid(_In_ winrt::hstring const& text, _Out_ winrt::guid& value) noexcept
    {
        value = winrt::guid{};

        if (text.empty()) return false;

        try
        {
            value = winrt::guid{ std::wstring_view{ text } };
            return true;
        }
        catch (...)
        {
            value = winrt::guid{};
            return false;
        }
    }

    // The entry key form used in the configuration file. Creation and removal must agree on it,
    // because the file merge matches keys as text.
    inline winrt::hstring EntryKey(_In_ winrt::guid const& entryId)
    {
        return winrt::hstring{ internal::GuidToString(entryId) };
    }

    // a transport section wrapped the way MidiServiceTransportPluginConfigManager expects it
    inline json::JsonObject WrapTransportSection(_In_ json::JsonObject const& section)
    {
        json::JsonObject transports;
        transports.SetNamedValue(winrt::hstring{ MIDI_RTP_TRANSPORT_ID }, section);

        json::JsonObject wrapper;
        wrapper.SetNamedValue(MIDI_CONFIG_JSON_TRANSPORT_PLUGIN_SETTINGS_OBJECT, transports);

        return wrapper;
    }
}
