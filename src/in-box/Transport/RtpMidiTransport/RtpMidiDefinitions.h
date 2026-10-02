// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// What the customer configured: the hosts this PC offers and the remotes it connects
// to. Runtime state lives in the endpoint manager.
// ============================================================================

#pragma once

struct GuidLess
{
    bool operator()(_In_ GUID const& left, _In_ GUID const& right) const noexcept
    {
        return memcmp(&left, &right, sizeof(GUID)) < 0;
    }
};

enum class RtpMidiRemoteClientPolicy
{
    AllowAny,
    RequireApproval,
};

// This PC, offered on the network for remotes to connect to
struct RtpMidiHostDefinition
{
    GUID EntryId{};

    // What remotes show in their connection lists
    std::wstring Name;

    // The DNS-SD label. The name when empty.
    std::wstring ServiceInstanceName;

    // 0 means any free port
    uint16_t Port{ MIDI_RTP_DEFAULT_HOST_PORT };
    bool AllowPortFallback{ true };

    bool Advertise{ true };
    bool Enabled{ true };
    bool SendRecoveryJournal{ true };

    // Changing it never restarts the host, so connections already made stay up
    RtpMidiRemoteClientPolicy RemoteClientPolicy{ RtpMidiRemoteClientPolicy::AllowAny };

    // The adapter the host is limited to, or GUID_NULL for every adapter. Found by its id, then
    // by its hardware address. The name is only shown while the adapter is missing.
    GUID NetworkAdapterId{};
    std::wstring NetworkAdapterName;
    std::wstring NetworkAdapterPhysicalAddress;

    // When the adapter is missing: run on every adapter until it is back, instead of waiting
    bool AllowNetworkAdapterFallback{ true };

    bool IsLimitedToNetworkAdapter() const noexcept
    {
        return !IsEqualGUID(NetworkAdapterId, GUID_NULL) || !NetworkAdapterPhysicalAddress.empty();
    }

    std::wstring EffectiveServiceInstanceName() const
    {
        return ServiceInstanceName.empty() ? Name : ServiceInstanceName;
    }
};

// A remote host this PC connects to
struct RtpMidiClientDefinition
{
    GUID EntryId{};

    // What the remote shows for this PC
    std::wstring Name;

    // Either an advertised remote, found by its DNS-SD label...
    std::wstring RemoteServiceInstanceName;

    // ...or one at a fixed address, which may also be a host name
    std::wstring RemoteAddress;
    uint16_t RemotePort{ MIDI_RTP_DEFAULT_HOST_PORT };

    std::wstring CustomEndpointName;

    bool AutoReconnect{ true };
    bool Enabled{ true };
    bool SendRecoveryJournal{ true };

    bool IsDirect() const noexcept { return !RemoteAddress.empty(); }
};


// The configuration file is writable by a standard user and commands come from any client, so
// everything read here is untrusted. The WinRT JSON "default" accessors only cover a missing key:
// they throw when the key holds another type, and a throw escaping a transport ends the service.
namespace RtpMidiJson
{
    inline bool TryGetValue(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ json::JsonValueType const type,
        _Out_ json::IJsonValue& value) noexcept
    {
        value = nullptr;

        try
        {
            if (parent == nullptr) return false;

            winrt::hstring const name{ key };
            if (!parent.HasKey(name)) return false;

            auto const found = parent.Lookup(name);
            if (found == nullptr || found.ValueType() != type) return false;

            value = found;
            return true;
        }
        catch (...)
        {
            value = nullptr;
            return false;
        }
    }

    inline bool TryGetObject(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _Out_ json::JsonObject& value) noexcept
    {
        value = nullptr;

        try
        {
            json::IJsonValue found{ nullptr };
            if (!TryGetValue(parent, key, json::JsonValueType::Object, found)) return false;

            value = found.GetObject();
            return value != nullptr;
        }
        catch (...)
        {
            value = nullptr;
            return false;
        }
    }

    inline bool TryGetArray(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _Out_ json::JsonArray& value) noexcept
    {
        value = nullptr;

        try
        {
            json::IJsonValue found{ nullptr };
            if (!TryGetValue(parent, key, json::JsonValueType::Array, found)) return false;

            value = found.GetArray();
            return value != nullptr;
        }
        catch (...)
        {
            value = nullptr;
            return false;
        }
    }

    inline std::wstring GetString(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ std::wstring const& defaultValue = {}) noexcept
    {
        try
        {
            json::IJsonValue found{ nullptr };
            if (!TryGetValue(parent, key, json::JsonValueType::String, found)) return defaultValue;

            return std::wstring{ found.GetString() };
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    inline bool GetBoolean(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _In_ bool const defaultValue) noexcept
    {
        try
        {
            json::IJsonValue found{ nullptr };
            if (!TryGetValue(parent, key, json::JsonValueType::Boolean, found)) return defaultValue;

            return found.GetBoolean();
        }
        catch (...)
        {
            return defaultValue;
        }
    }

    // A port as a JSON number or as a string of digits, 1024 to 65534 because the data port is the
    // next one up. "auto" and a missing key are reported through isAuto and isPresent, so the
    // caller decides what they mean.
    inline bool TryGetPort(
        _In_ json::JsonObject const& parent,
        _In_ std::wstring_view const key,
        _Out_ uint16_t& port,
        _Out_ bool& isAuto,
        _Out_ bool& isPresent) noexcept
    {
        port = 0;
        isAuto = false;
        isPresent = false;

        try
        {
            if (parent == nullptr || !parent.HasKey(winrt::hstring{ key })) return true;

            isPresent = true;

            json::IJsonValue found{ nullptr };

            if (TryGetValue(parent, key, json::JsonValueType::Number, found))
            {
                auto const number = found.GetNumber();
                if (!(number >= 1024 && number <= 65534) || number != static_cast<double>(static_cast<uint32_t>(number))) return false;

                port = static_cast<uint16_t>(number);
                return true;
            }

            if (TryGetValue(parent, key, json::JsonValueType::String, found))
            {
                std::wstring const text{ found.GetString() };

                if (_wcsicmp(text.c_str(), MIDI_CONFIG_JSON_RTP_MIDI_PORT_VALUE_AUTO) == 0)
                {
                    isAuto = true;
                    return true;
                }

                if (text.empty() || text.size() > 5) return false;
                if (!std::all_of(text.begin(), text.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; })) return false;

                auto const value = std::stoul(text);
                if (value < 1024 || value > 65534) return false;

                port = static_cast<uint16_t>(value);
                return true;
            }

            return false;
        }
        catch (...)
        {
            return false;
        }
    }

    // The keys of a create or remove section are entry identifiers
    inline bool TryParseEntryIdentifier(_In_ std::wstring const& text, _Out_ GUID& entryId) noexcept
    {
        entryId = GUID{};

        try
        {
            return internal::TryParseGuidString(text, entryId);
        }
        catch (...)
        {
            entryId = GUID{};
            return false;
        }
    }
}
