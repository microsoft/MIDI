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
#include "..\..\..\..\Transport\BleMidi2Transport\bluetooth_json_defs.h"
#include "..\..\..\..\Transport\BleMidi2Transport\bluetooth_transport_error_codes.h"

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
