// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiBluetoothIncomingTimingConfig.h"
#include "Transports.Bluetooth.MidiBluetoothIncomingTimingConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothIncomingTimingConfig::MidiBluetoothIncomingTimingConfig(bluetooth::MidiBluetoothIncomingTiming const incomingTiming)
    {
        m_incomingTiming = incomingTiming;
    }

    _Use_decl_annotations_
    MidiBluetoothIncomingTimingConfig::MidiBluetoothIncomingTimingConfig(
        winrt::hstring const& bluetoothDeviceId,
        bluetooth::MidiBluetoothIncomingTiming const incomingTiming)
    {
        m_bluetoothDeviceId = bluetoothDeviceId;
        m_incomingTiming = incomingTiming;
    }

    json::JsonObject MidiBluetoothIncomingTimingConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            auto const value = json::JsonValue::CreateStringValue(
                btinternal::IncomingTimingToJsonString(m_incomingTiming));

            if (m_bluetoothDeviceId.empty())
            {
                configJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INCOMING_TIMING_KEY, value);

                return configJson;
            }

            json::JsonObject deviceJson;

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY,
                json::JsonValue::CreateStringValue(btinternal::BluetoothDeviceIdForConfig(m_bluetoothDeviceId)));

            deviceJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INCOMING_TIMING_KEY, value);

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