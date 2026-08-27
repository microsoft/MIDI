#include "pch.h"
#include "MidiBluetoothPeripheralClientListConfig.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralClientListConfig.g.cpp"

#include "midi_bluetooth_utility.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    MidiBluetoothPeripheralClientListConfig::MidiBluetoothPeripheralClientListConfig(
        bluetooth::MidiBluetoothPeripheralStatus const& currentStatus)
    {
        if (currentStatus != nullptr)
        {
            m_allowed = currentStatus.AllowedClients();
            m_denied = currentStatus.DeniedClients();
        }
    }

    namespace
    {
        json::JsonArray BuildArray(
            _In_ collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> const& clients) noexcept
        {
            json::JsonArray entries;

            if (clients == nullptr)
            {
                return entries;
            }

            try
            {
                for (auto const& client : clients)
                {
                    if (client == nullptr || client.BluetoothAddress().empty())
                    {
                        continue;
                    }

                    json::JsonObject entry;

                    entry.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY,
                        json::JsonValue::CreateStringValue(client.BluetoothAddress()));

                    entry.SetNamedValue(
                        MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY,
                        json::JsonValue::CreateStringValue(client.Name()));

                    entries.Append(entry);
                }
            }
            catch (...)
            {
            }

            return entries;
        }
    }

    json::JsonObject MidiBluetoothPeripheralClientListConfig::ConfigJson()
    {
        json::JsonObject configJson;

        try
        {
            json::JsonObject peripheralJson;

            // Both lists are written every time. The configuration file replaces an array whole
            // rather than merging it, so omitting one would erase it.
            peripheralJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ALLOWED_CLIENTS_KEY, BuildArray(m_allowed));

            peripheralJson.SetNamedValue(
                MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DENIED_CLIENTS_KEY, BuildArray(m_denied));

            configJson.SetNamedValue(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_KEY, peripheralJson);
        }
        catch (...)
        {
        }

        return configJson;
    }
}
