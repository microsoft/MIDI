// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiBluetoothOfflineRetentionConfig.h"
#include "Transports.Bluetooth.MidiBluetoothOfflineRetentionConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothOfflineRetentionConfig::MidiBluetoothOfflineRetentionConfig(int32_t const retentionSeconds)
    {
        m_retentionSeconds = retentionSeconds;
    }

    _Use_decl_annotations_
    MidiBluetoothOfflineRetentionConfig::MidiBluetoothOfflineRetentionConfig(
        winrt::hstring const& bluetoothDeviceId,
        int32_t const retentionSeconds)
    {
        m_bluetoothDeviceId = bluetoothDeviceId;
        m_retentionSeconds = retentionSeconds;
    }

    json::JsonObject MidiBluetoothOfflineRetentionConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            auto const value = json::JsonValue::CreateStringValue(
                btinternal::OfflineRetentionToJsonString(m_retentionSeconds));

            if (m_bluetoothDeviceId.empty())
            {
                configJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY, value);

                return configJson;
            }

            json::JsonObject deviceJson;

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY,
                json::JsonValue::CreateStringValue(m_bluetoothDeviceId));

            deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY, value);

            json::JsonArray devicesJson;
            devicesJson.Append(deviceJson);

            configJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY, devicesJson);
        }
        catch (...)
        {
        }

        return configJson;
    }
}
