#include "pch.h"
#include "MidiBluetoothPeripheralStatus.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralStatus.g.cpp"

#include "MidiBluetoothPeripheralClient.h"
#include "MidiBluetoothRememberedClient.h"
#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    namespace
    {
        collections::IVectorView<bluetooth::MidiBluetoothRememberedClient> ReadRememberedClients(
            _In_ json::JsonObject const& peripheralJson,
            _In_ winrt::hstring const& key) noexcept
        {
            auto results = winrt::single_threaded_vector<bluetooth::MidiBluetoothRememberedClient>();

            try
            {
                if (peripheralJson.TryLookup(key) == nullptr)
                {
                    return results.GetView();
                }

                auto const entries = peripheralJson.GetNamedArray(key, nullptr);

                if (entries == nullptr)
                {
                    return results.GetView();
                }

                for (auto const& entry : entries)
                {
                    if (entry == nullptr || entry.ValueType() != json::JsonValueType::Object)
                    {
                        continue;
                    }

                    auto const entryObject = entry.GetObject();

                    auto client = winrt::make_self<MidiBluetoothRememberedClient>();

                    client->InternalInitialize(
                        entryObject.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY, L""),
                        entryObject.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY, L""));

                    results.Append(*client);
                }
            }
            catch (...)
            {
            }

            return results.GetView();
        }
    }
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

            m_clientPolicy = btinternal::ClientPolicyFromJsonString(
                peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_CLIENT_POLICY_KEY, L""));

            m_allowedClients = ReadRememberedClients(
                peripheralJson, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ALLOWED_CLIENTS_KEY);

            m_deniedClients = ReadRememberedClients(
                peripheralJson, MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DENIED_CLIENTS_KEY);

            m_isClientConnected = peripheralJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY, false);

            m_endpointDeviceId = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY, L"");
            m_endpointDeviceInstanceId = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY, L"");

            m_messagesReceived = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY, 0.0));
            m_messagesSent = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY, 0.0));

            m_packetsReceived = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PACKETS_RECEIVED_KEY, 0.0));
            m_packetsSent = static_cast<uint64_t>(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PACKETS_SENT_KEY, 0.0));

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
