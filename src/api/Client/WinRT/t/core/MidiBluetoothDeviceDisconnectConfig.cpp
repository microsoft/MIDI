#include "pch.h"
#include "MidiBluetoothDeviceDisconnectConfig.h"
#include "Transports.Bluetooth.MidiBluetoothDeviceDisconnectConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothDeviceDisconnectConfig::MidiBluetoothDeviceDisconnectConfig(winrt::hstring const& bluetoothDeviceId)
    {
        m_bluetoothDeviceId = bluetoothDeviceId;
    }

    _Use_decl_annotations_
    MidiBluetoothDeviceDisconnectConfig::MidiBluetoothDeviceDisconnectConfig(
        winrt::hstring const& bluetoothDeviceId,
        bool const removeFromConfiguration)
    {
        m_bluetoothDeviceId = bluetoothDeviceId;
        m_removeFromConfiguration = removeFromConfiguration;
    }

    json::JsonObject MidiBluetoothDeviceDisconnectConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            if (m_removeFromConfiguration)
            {
                json::JsonArray idsJson;
                idsJson.Append(json::JsonValue::CreateStringValue(m_bluetoothDeviceId));

                json::JsonObject removeJson;
                removeJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICES_ARRAY_KEY, idsJson);

                configJson.SetNamedValue(MIDI_CONFIG_JSON_ENDPOINT_COMMON_REMOVE_KEY, removeJson);

                return configJson;
            }

            json::JsonObject deviceJson;

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY,
                json::JsonValue::CreateStringValue(m_bluetoothDeviceId));

            deviceJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ENABLED_KEY,
                json::JsonValue::CreateBooleanValue(false));

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
