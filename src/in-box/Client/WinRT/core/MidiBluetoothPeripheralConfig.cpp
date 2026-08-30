#include "pch.h"
#include "MidiBluetoothPeripheralConfig.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothPeripheralConfig::MidiBluetoothPeripheralConfig(bluetooth::MidiBluetoothProtocol const protocol)
    {
        m_protocol = protocol;
    }

    json::JsonObject MidiBluetoothPeripheralConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            json::JsonObject peripheralJson;

            peripheralJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ENABLED_KEY,
                json::JsonValue::CreateBooleanValue(m_isEnabled));

            if (m_isEnabled)
            {
                peripheralJson.SetNamedValue(
                    MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY,
                    json::JsonValue::CreateStringValue(btinternal::ProtocolToJsonString(m_protocol)));
            }

            configJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, peripheralJson);
        }
        catch (...)
        {
        }

        return configJson;
    }
}
