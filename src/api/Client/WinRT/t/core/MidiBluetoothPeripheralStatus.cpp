#include "pch.h"
#include "MidiBluetoothPeripheralStatus.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralStatus.g.cpp"

#include "MidiBluetoothPeripheralClient.h"
#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothPeripheralStatus::InternalInitializeFromJson(json::JsonObject const& peripheralJson) noexcept
    {
        if (peripheralJson == nullptr)
        {
            return;
        }

        try
        {
            m_isRunning = peripheralJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_IS_RUNNING_KEY, false);

            m_protocol = btinternal::ProtocolFromJsonString(
                peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_PROTOCOL_KEY, L""));

            m_advertisedName = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_ADVERTISED_NAME_KEY, L"");

            m_subscribedClientCount = static_cast<uint32_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PERIPHERAL_CLIENT_COUNT_KEY, 0.0));

            m_isClientConnected = peripheralJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY, false);

            m_endpointDeviceId = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY, L"");
            m_endpointDeviceInstanceId = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY, L"");

            m_messagesReceived = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY, 0.0));
            m_messagesSent = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY, 0.0));

            if (m_isClientConnected)
            {
                auto client = winrt::make_self<implementation::MidiBluetoothPeripheralClient>();
                client->InternalInitializeFromJson(peripheralJson);

                m_connectedClient = *client;
            }
        }
        catch (...)
        {
        }
    }
}
