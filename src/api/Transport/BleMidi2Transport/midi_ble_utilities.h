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

        // The interval the link negotiated, in units of 1.25 ms. Zero when not connected.
        uint16_t ConnectionIntervalUnits{ 0 };

        // Deterministic, so a customization can name a device before its endpoint exists.
        winrt::hstring EndpointDeviceInstanceId{ };

        // Connecting happens on a background worker long after the command returns, so the last
        // failure is kept here. Without it a failed connect is completely silent.
        int32_t LastConnectErrorHresult{ 0 };
        winrt::hstring LastConnectErrorDetail{ };

        // filled in from the live connection when one exists
        uint64_t MessagesReceived{ 0 };
        uint64_t MessagesSent{ 0 };
        int32_t LastSendErrorHresult{ 0 };

        // computed when the list is taken, because both are relative to now
        uint64_t LastSeenAgoMilliseconds{ 0 };
        bool IsPresent{ false };

        // An endpoint can exist for a device which has gone away. Keeping these separate is what
        // makes a silent disconnect visible instead of leaving apps holding a dead endpoint.
        bool HasEndpoint{ false };
    };
}

namespace MidiBleUtilities
{
    using namespace ::winrt::Windows::Devices::Bluetooth;
    using namespace ::winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;

    // A GATT call against a device which has gone to sleep or out of range blocks for the full
    // Bluetooth timeout. Service shutdown joins the threads which make these calls, so an
    // unbounded wait here keeps the whole midisrv process alive long after the service stopped.
    inline constexpr uint32_t BleOperationTimeoutMilliseconds = 5000;
    inline constexpr uint32_t BleDataOperationTimeoutMilliseconds = 2000;
    inline constexpr uint32_t BleTeardownOperationTimeoutMilliseconds = 1000;

    template<typename TResult>
    inline TResult AwaitWithTimeout(
        _In_ ::winrt::Windows::Foundation::IAsyncOperation<TResult> const& operation,
        _In_ uint32_t const timeoutMilliseconds,
        _In_ TResult const onTimeout)
    {
        if (operation == nullptr)
        {
            return onTimeout;
        }

        try
        {
            if (operation.wait_for(std::chrono::milliseconds{ timeoutMilliseconds }) ==
                ::winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                return operation.GetResults();
            }

            // Best effort. The operation keeps its own references and unwinds on its own, so
            // abandoning it is safe even when the cancel is ignored.
            operation.Cancel();
        }
        CATCH_LOG();

        return onTimeout;
    }

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

    inline winrt::hstring BluetoothAddressTypeToString(_In_ BluetoothAddressType const addressType)
    {
        switch (addressType)
        {
        case BluetoothAddressType::Public:      return L"public";
        case BluetoothAddressType::Random:      return L"random";
        default:                                return L"unspecified";
        }
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

    // Returns null for SystemDefault, meaning make no request and leave the radio alone.
    inline BluetoothLEPreferredConnectionParameters GetPreferredConnectionParameters(
        _In_ MidiBleProtocol::ConnectionParameterPreference const preference)
    {
        switch (preference)
        {
        case MidiBleProtocol::ConnectionParameterPreference::ThroughputOptimized:
            return BluetoothLEPreferredConnectionParameters::ThroughputOptimized();

        case MidiBleProtocol::ConnectionParameterPreference::Balanced:
            return BluetoothLEPreferredConnectionParameters::Balanced();

        case MidiBleProtocol::ConnectionParameterPreference::PowerOptimized:
            return BluetoothLEPreferredConnectionParameters::PowerOptimized();

        default:
            return nullptr;
        }
    }

    inline BluetoothLEDevice GetBleDeviceFromEnumerationDeviceId(_In_ std::wstring deviceId)
    {
        return AwaitWithTimeout(
            BluetoothLEDevice::FromIdAsync(winrt::to_hstring(deviceId.c_str())),
            BleOperationTimeoutMilliseconds,
            BluetoothLEDevice{ nullptr });
    }

    // The name a remote Central sees for this PC. The GATT service provider puts the system's
    // Bluetooth name in the advertisement and gives an application no way to override it, and
    // Windows takes that name from the computer name, so this is reported rather than configured.
    inline winrt::hstring GetLocalBluetoothName()
    {
        wchar_t computerName[MAX_COMPUTERNAME_LENGTH + 1]{ 0 };
        DWORD computerNameLength = ARRAYSIZE(computerName);

        if (GetComputerNameW(computerName, &computerNameLength))
        {
            return winrt::hstring{ computerName };
        }

        return L"";
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


    inline GattDeviceService GetBleMidiServiceFromDevice(
        _In_ BluetoothLEDevice bleDevice,
        _Out_ GattCommunicationStatus& status)
    {
        status = GattCommunicationStatus::Unreachable;

        if (bleDevice == nullptr)
        {
            return nullptr;
        }

        winrt::guid bleServiceUuid{ MidiBleProtocol::MidiServiceUuid };

        auto gattServicesResult = AwaitWithTimeout(
            bleDevice.GetGattServicesForUuidAsync(bleServiceUuid, BluetoothCacheMode::Uncached),
            BleOperationTimeoutMilliseconds,
            GattDeviceServicesResult{ nullptr });

        if (gattServicesResult == nullptr)
        {
            return nullptr;
        }

        status = gattServicesResult.Status();

        if (gattServicesResult.Status() == GattCommunicationStatus::Success && gattServicesResult.Services().Size() > 0)
        {
            return gattServicesResult.Services().GetAt(0);
        }

        return nullptr;
    }
}

#endif