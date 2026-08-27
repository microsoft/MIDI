#include "pch.h"
#include "MidiBluetoothDeviceConnectConfig.h"
#include "Transports.Bluetooth.MidiBluetoothDeviceConnectConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothDeviceConnectConfig::MidiBluetoothDeviceConnectConfig(winrt::hstring const& bluetoothDeviceId)
    {
        m_bluetoothDeviceId = bluetoothDeviceId;
    }

    json::JsonObject MidiBluetoothDeviceConnectConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            json::JsonObject deviceJson;

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY,
                json::JsonValue::CreateStringValue(m_bluetoothDeviceId));

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY,
                json::JsonValue::CreateBooleanValue(true));

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
