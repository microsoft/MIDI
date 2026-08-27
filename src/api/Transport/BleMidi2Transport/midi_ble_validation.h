// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_VALIDATION_H
#define MIDI_BLE_VALIDATION_H

// Everything here is decided from a string, a number or a json value, and never touches the
// radio, the service or COM. Kept apart from midi_ble_utilities.h so it can be unit tested
// without any of that.

#include <cstdint>
#include <string>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>

#include "bluetooth_json_defs.h"

namespace MidiBleProtocol
{
    // The configuration file is writable by a standard user, so everything read out of it is
    // untrusted. The WinRT json "default" accessors only cover a MISSING key: they throw when the
    // key is present but holds another type, and a throw escaping a transport kills the service.
    namespace SafeJson
    {
        inline bool TryGetObject(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _Out_ ::winrt::Windows::Data::Json::JsonObject& value) noexcept
        {
            value = nullptr;

            try
            {
                if (parent == nullptr)
                {
                    return false;
                }

                ::winrt::hstring const keyName{ key };

                if (!parent.HasKey(keyName))
                {
                    return false;
                }

                auto const found = parent.Lookup(keyName);

                if (found == nullptr || found.ValueType() != ::winrt::Windows::Data::Json::JsonValueType::Object)
                {
                    return false;
                }

                value = parent.GetNamedObject(keyName);

                return value != nullptr;
            }
            catch (...)
            {
                value = nullptr;
                return false;
            }
        }

        inline bool TryGetArray(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _Out_ ::winrt::Windows::Data::Json::JsonArray& value) noexcept
        {
            value = nullptr;

            try
            {
                if (parent == nullptr)
                {
                    return false;
                }

                ::winrt::hstring const keyName{ key };

                if (!parent.HasKey(keyName))
                {
                    return false;
                }

                auto const found = parent.Lookup(keyName);

                if (found == nullptr || found.ValueType() != ::winrt::Windows::Data::Json::JsonValueType::Array)
                {
                    return false;
                }

                value = parent.GetNamedArray(keyName);

                return value != nullptr;
            }
            catch (...)
            {
                value = nullptr;
                return false;
            }
        }

        inline ::winrt::hstring GetString(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _In_ ::winrt::hstring const& defaultValue = ::winrt::hstring{}) noexcept
        {
            try
            {
                if (parent == nullptr)
                {
                    return defaultValue;
                }

                ::winrt::hstring const keyName{ key };

                if (!parent.HasKey(keyName))
                {
                    return defaultValue;
                }

                auto const found = parent.Lookup(keyName);

                if (found == nullptr || found.ValueType() != ::winrt::Windows::Data::Json::JsonValueType::String)
                {
                    return defaultValue;
                }

                return found.GetString();
            }
            catch (...)
            {
                return defaultValue;
            }
        }

        inline bool GetBoolean(
            _In_ ::winrt::Windows::Data::Json::JsonObject const& parent,
            _In_ std::wstring_view const key,
            _In_ bool const defaultValue) noexcept
        {
            try
            {
                if (parent == nullptr)
                {
                    return defaultValue;
                }

                ::winrt::hstring const keyName{ key };

                if (!parent.HasKey(keyName))
                {
                    return defaultValue;
                }

                auto const found = parent.Lookup(keyName);

                if (found == nullptr || found.ValueType() != ::winrt::Windows::Data::Json::JsonValueType::Boolean)
                {
                    return defaultValue;
                }

                return found.GetBoolean();
            }
            catch (...)
            {
                return defaultValue;
            }
        }
    }

    enum class Protocol : uint8_t
    {
        Unknown = 0,
        Midi1 = 1,
        Midi2Ump = 2,
    };

    enum class NativeDataFormat : uint8_t
    {
        Unknown = 0,
        TimestampedMidi1ByteStream = 1,
        UniversalMidiPacket = 2,
    };

    // Both specifications require an interval of 15 ms or less and prefer the lowest both ends
    // support. Windows exposes three presets and no way to name an interval, and the
    // throughput-optimized one asks for 15 ms as both the floor and the ceiling, so which of
    // these actually produces the lowest interval has to be measured rather than assumed.
    enum class ConnectionParameterPreference : uint8_t
    {
        SystemDefault = 0,
        ThroughputOptimized = 1,
        Balanced = 2,
        PowerOptimized = 3,
    };

    // What the radio on this machine can actually do. Probed once at start up and reported, so a
    // machine with no Bluetooth, or with a radio which cannot act as a peripheral, explains itself
    // instead of looking like a transport which silently does nothing.
    struct RadioCapabilities
    {
        bool RadioPresent{ false };
        bool LowEnergySupported{ false };
        bool CentralRoleSupported{ false };
        bool PeripheralRoleSupported{ false };

        // Advertisement offload is not required for anything here, but it is the clearest signal
        // of how capable the radio is when a device will not connect.
        uint32_t MaxAdvertisementDataLength{ 0 };

        bool CanConnectToDevices() const noexcept
        {
            return RadioPresent && LowEnergySupported && CentralRoleSupported;
        }

        bool CanPublishPeripheral() const noexcept
        {
            return RadioPresent && LowEnergySupported && PeripheralRoleSupported;
        }
    };
}

namespace MidiBleUtilities
{
    inline winrt::hstring ProtocolToJsonString(_In_ MidiBleProtocol::Protocol const protocol)
    {
        switch (protocol)
        {
        case MidiBleProtocol::Protocol::Midi1:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1;

        case MidiBleProtocol::Protocol::Midi2Ump:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP;

        default:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_UNKNOWN;
        }
    }

    inline winrt::hstring NativeDataFormatToJsonString(_In_ MidiBleProtocol::NativeDataFormat const nativeDataFormat)
    {
        switch (nativeDataFormat)
        {
        case MidiBleProtocol::NativeDataFormat::TimestampedMidi1ByteStream:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_MIDI1;

        case MidiBleProtocol::NativeDataFormat::UniversalMidiPacket:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UMP;

        default:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_NATIVE_DATA_FORMAT_VALUE_UNKNOWN;
        }
    }

    // Apple, and to a lesser extent Android, withhold the user-assigned device name from an
    // unpaired peer and report the model instead. Those names are useless for telling two devices
    // apart and collide constantly, so they are worth pointing out to the user. Matched whole, so
    // a real name which merely contains one of these is not flagged.
    inline bool IsGenericDeviceName(_In_ winrt::hstring const& name)
    {
        static wchar_t const* const genericNames[] =
        {
            L"iPhone", L"iPad", L"iPod", L"iPod touch",
            L"Apple Watch", L"Mac", L"MacBook", L"MacBook Pro", L"MacBook Air", L"iMac",
            L"Android", L"Android Phone", L"Android Tablet",
            L"Phone", L"Tablet", L"Bluetooth", L"BLE MIDI"
        };

        if (name.empty())
        {
            return false;
        }

        for (auto const& generic : genericNames)
        {
            if (_wcsicmp(name.c_str(), generic) == 0)
            {
                return true;
            }
        }

        return false;
    }

    inline winrt::hstring ConnectionParameterPreferenceToJsonString(
        _In_ MidiBleProtocol::ConnectionParameterPreference const preference)
    {
        switch (preference)
        {
        case MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_THROUGHPUT;

        case MidiBleProtocol::ConnectionParameterPreference::Balanced:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_BALANCED;

        case MidiBleProtocol::ConnectionParameterPreference::PowerOptimized:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_POWER;

        default:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_DEFAULT;
        }
    }

    inline MidiBleProtocol::ConnectionParameterPreference ConnectionParameterPreferenceFromJsonString(
        _In_ winrt::hstring const& value,
        _In_ MidiBleProtocol::ConnectionParameterPreference const fallback)
    {
        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_THROUGHPUT)
        {
            return MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized;
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_BALANCED)
        {
            return MidiBleProtocol::ConnectionParameterPreference::Balanced;
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_POWER)
        {
            return MidiBleProtocol::ConnectionParameterPreference::PowerOptimized;
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CONNECTION_PARAMETERS_VALUE_DEFAULT)
        {
            return MidiBleProtocol::ConnectionParameterPreference::SystemDefault;
        }

        return fallback;
    }

    // The Bluetooth address is the only identifier both the advertisement watcher and the GATT
    // device watcher can supply, so it is what commands, discovery and device instance ids all
    // key on.
    inline winrt::hstring FormatBluetoothAddress(_In_ uint64_t const address)
    {
        wchar_t buffer[13]{ };

        swprintf_s(buffer, ARRAYSIZE(buffer), L"%012llX", address & 0x0000FFFFFFFFFFFFULL);

        return winrt::hstring{ buffer };
    }

    // Accepts the bare hex form this transport emits as well as the colon and dash separated
    // forms the Windows property system and users produce.
    inline bool TryParseBluetoothAddress(_In_ std::wstring const& value, _Out_ uint64_t& address)
    {
        address = 0;

        uint8_t digitCount{ 0 };

        for (auto const& ch : value)
        {
            if (ch == L':' || ch == L'-' || ch == L' ')
            {
                continue;
            }

            uint64_t digit{ 0 };

            if (ch >= L'0' && ch <= L'9')       digit = static_cast<uint64_t>(ch - L'0');
            else if (ch >= L'A' && ch <= L'F')  digit = static_cast<uint64_t>(ch - L'A') + 10;
            else if (ch >= L'a' && ch <= L'f')  digit = static_cast<uint64_t>(ch - L'a') + 10;
            else return false;

            if (digitCount >= 12)
            {
                return false;
            }

            address = (address << 4) | digit;
            digitCount++;
        }

        return digitCount > 0;
    }

    // Stricter than TryParseBluetoothAddress, which accepts a partial address so that a person can
    // type one. A value out of the configuration file has to be a whole address or it would sit in
    // the connect list forever, matching nothing.
    inline bool IsWellFormedBluetoothDeviceId(_In_ std::wstring const& value) noexcept
    {
        uint8_t digitCount{ 0 };

        for (auto const& ch : value)
        {
            if (ch == L':' || ch == L'-' || ch == L' ')
            {
                continue;
            }

            if (!((ch >= L'0' && ch <= L'9') || (ch >= L'A' && ch <= L'F') || (ch >= L'a' && ch <= L'f')))
            {
                return false;
            }

            if (++digitCount > 12)
            {
                return false;
            }
        }

        return digitCount == 12;
    }
}

#endif // MIDI_BLE_VALIDATION_H
