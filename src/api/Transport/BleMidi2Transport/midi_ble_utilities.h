// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://github.com/microsoft/MIDI/
// ============================================================================

#pragma once

#ifndef MIDI_BLE_UTILITIES_H
#define MIDI_BLE_UTILITIES_H

namespace MidiBleProtocol
{
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

    inline constexpr wchar_t MidiServiceUuid[] = L"{03B80E5A-EDE8-4B33-A751-6CE34EC4C700}";
    inline constexpr wchar_t Midi1DataIoCharacteristicUuid[] = L"{7772E5DB-3868-4112-A1A9-F2669D106BF3}";
    inline constexpr wchar_t Midi2UmpCharacteristicUuid[] = L"{C3B10ECF-88F5-4F7D-BFFA-8AD2C91FBAFE}";

    // A device is dropped from the available list when it has not advertised for this long.
    inline constexpr uint64_t DeviceStaleAfterMilliseconds = 60000;

    struct DiscoveredDevice
    {
        uint64_t BluetoothAddress{ 0 };
        winrt::hstring Id{ };                       // the 12 hex digit address, and the key for every command
        winrt::hstring Name{ };
        winrt::hstring GattServiceDeviceId{ };      // known only once the system has enumerated the GATT service
        Protocol SelectedProtocol{ Protocol::Unknown };
        NativeDataFormat NativeDataFormat{ NativeDataFormat::Unknown };
        bool IsPaired{ false };
        bool IsConnected{ false };
        int16_t LastSignalStrengthDbm{ 0 };
        uint64_t LastSeenTimestamp{ 0 };
        winrt::hstring EndpointDeviceId{ };
    };
}

namespace MidiBleUtilities
{
    using namespace ::winrt::Windows::Devices::Bluetooth;
    using namespace ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

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


    inline BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
    {
        //    std::cout << __FUNCTION__ << std::endl;

        auto device = BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())).get();

        return device;
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


    inline GattDeviceService GetBleMidiServiceFromDevice(_In_ BluetoothLEDevice bleDevice)
    {
        if (bleDevice == nullptr)
        {
            return nullptr;
        }

        winrt::guid bleServiceUuid{ MidiBleProtocol::MidiServiceUuid };

        auto gattServicesResult = bleDevice.GetGattServicesForUuidAsync(bleServiceUuid, BluetoothCacheMode::Uncached).get();

        if (gattServicesResult.Status() == GattCommunicationStatus::Success && gattServicesResult.Services().Size() > 0)
        {
            return gattServicesResult.Services().GetAt(0);
        }

        return nullptr;
    }
}

#endif