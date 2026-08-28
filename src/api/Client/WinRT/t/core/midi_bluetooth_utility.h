// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// The json keys and error codes are shared with the transport rather than restated here, so the
// two cannot drift
#include "..\..\..\..\Transport\BluetoothMidiTransport\bluetooth_json_defs.h"
#include "..\..\..\..\Transport\BluetoothMidiTransport\bluetooth_transport_error_codes.h"

namespace Windows::Devices::Midi2::Transports::Bluetooth::Internal
{
    inline foundation::TimeSpan TimeSpanFromMilliseconds(_In_ double const milliseconds) noexcept
    {
        if (milliseconds <= 0.0)
        {
            return foundation::TimeSpan{ 0 };
        }

        return foundation::TimeSpan{ static_cast<int64_t>(milliseconds * 10000.0) };
    }

    // Inverse of the service's FileTimeToIso8601. The wire format is ISO 8601 UTC with the full
    // 100ns FILETIME resolution, for example 2026-08-12T01:23:45.6789012Z. Anything unparseable
    // becomes a zero DateTime, which is also what "nothing is waiting" means on the service side.
    inline foundation::DateTime RequestTimeFromJsonString(_In_ winrt::hstring const& value) noexcept
    {
        foundation::DateTime result{};

        if (value.empty())
        {
            return result;
        }

        uint32_t year{}, month{}, day{}, hour{}, minute{}, second{}, fraction{};

        if (swscanf_s(value.c_str(), L"%4u-%2u-%2uT%2u:%2u:%2u.%7uZ",
            &year, &month, &day, &hour, &minute, &second, &fraction) != 7)
        {
            return result;
        }

        SYSTEMTIME st{};
        st.wYear = static_cast<WORD>(year);
        st.wMonth = static_cast<WORD>(month);
        st.wDay = static_cast<WORD>(day);
        st.wHour = static_cast<WORD>(hour);
        st.wMinute = static_cast<WORD>(minute);
        st.wSecond = static_cast<WORD>(second);

        FILETIME ft{};

        if (!SystemTimeToFileTime(&st, &ft))
        {
            return result;
        }

        // whole seconds from the conversion, plus the sub-second ticks the service preserved
        uint64_t fileTime = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        fileTime += fraction;

        return winrt::clock::from_file_time(winrt::file_time{ fileTime });
    }

    inline bluetooth::MidiBluetoothPeripheralClientPolicy ClientPolicyFromJsonString(
        _In_ winrt::hstring const& value) noexcept
    {
        return value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_VALUE_ALLOW_ANY ?
            bluetooth::MidiBluetoothPeripheralClientPolicy::AllowAny :
            bluetooth::MidiBluetoothPeripheralClientPolicy::RequireApproval;
    }

    inline winrt::hstring ApprovalScopeToJsonString(
        _In_ bluetooth::MidiBluetoothApprovalScope const scope) noexcept
    {
        switch (scope)
        {
        case bluetooth::MidiBluetoothApprovalScope::Always:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_ALWAYS;

        case bluetooth::MidiBluetoothApprovalScope::UntilRestart:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_UNTIL_RESTART;

        default:
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_APPROVAL_SCOPE_VALUE_ONCE;
        }
    }

    inline bluetooth::MidiBluetoothProtocol ProtocolFromJsonString(_In_ winrt::hstring const& value) noexcept
    {
        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP)
        {
            return bluetooth::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump;
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1)
        {
            return bluetooth::MidiBluetoothProtocol::BluetoothLowEnergyMidi1;
        }

        return bluetooth::MidiBluetoothProtocol::Unknown;
    }

    inline winrt::hstring ProtocolToJsonString(_In_ bluetooth::MidiBluetoothProtocol const protocol) noexcept
    {
        switch (protocol)
        {
        case bluetooth::MidiBluetoothProtocol::BluetoothLowEnergyMidi2Ump:
            return winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI2_UMP };

        case bluetooth::MidiBluetoothProtocol::BluetoothLowEnergyMidi1:
            return winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_MIDI1 };

        default:
            return winrt::hstring{ MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PROTOCOL_VALUE_UNKNOWN };
        }
    }

    inline bluetooth::MidiBluetoothAddressType AddressTypeFromJsonString(_In_ winrt::hstring const& value) noexcept
    {
        auto const upper = internal::ToUpperTrimmedWStringCopy(std::wstring{ value });

        if (upper == L"PUBLIC")
        {
            return bluetooth::MidiBluetoothAddressType::Public;
        }

        if (upper == L"RANDOM")
        {
            return bluetooth::MidiBluetoothAddressType::Random;
        }

        return bluetooth::MidiBluetoothAddressType::Unknown;
    }

    // Anything above zero is a number of seconds; the named values cover everything else. Mirrors
    // the transport's parser, which is the only thing that writes these.
    inline int32_t OfflineRetentionFromJsonString(_In_ winrt::hstring const& value) noexcept
    {
        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_DEFAULT)
        {
            return static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::UseTransportDefault);
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_IMMEDIATE)
        {
            return static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::Immediate);
        }

        if (value == MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_ALWAYS)
        {
            return static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::KeepAlways);
        }

        uint64_t parsed{ 0 };

        for (auto const& ch : std::wstring{ value })
        {
            if (ch < L'0' || ch > L'9')
            {
                return static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::KeepAlways);
            }

            parsed = (parsed * 10) + static_cast<uint64_t>(ch - L'0');

            if (parsed > 86400)
            {
                return static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::KeepAlways);
            }
        }

        return static_cast<int32_t>(parsed);
    }

    inline winrt::hstring OfflineRetentionToJsonString(_In_ int32_t const seconds) noexcept
    {
        if (seconds == static_cast<int32_t>(bluetooth::MidiBluetoothOfflineRetention::UseTransportDefault))
        {
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_DEFAULT;
        }

        if (seconds < 0)
        {
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_ALWAYS;
        }

        if (seconds == 0)
        {
            return MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_VALUE_IMMEDIATE;
        }

        return winrt::to_hstring(seconds);
    }

    // The transport keys everything on the 12 hex digit address, so the numeric form is recovered
    // from the id rather than sent twice.
    inline uint64_t BluetoothAddressFromDeviceId(_In_ winrt::hstring const& bluetoothDeviceId) noexcept
    {
        uint64_t address{ 0 };
        uint8_t digitCount{ 0 };

        for (auto const& ch : std::wstring{ bluetoothDeviceId })
        {
            if (ch == L':' || ch == L'-' || ch == L' ')
            {
                continue;
            }

            uint64_t digit{ 0 };

            if (ch >= L'0' && ch <= L'9')        digit = static_cast<uint64_t>(ch - L'0');
            else if (ch >= L'A' && ch <= L'F')   digit = static_cast<uint64_t>(ch - L'A') + 10;
            else if (ch >= L'a' && ch <= L'f')   digit = static_cast<uint64_t>(ch - L'a') + 10;
            else return 0;

            if (digitCount >= 12)
            {
                return 0;
            }

            address = (address << 4) | digit;
            digitCount++;
        }

        return address;
    }
}
