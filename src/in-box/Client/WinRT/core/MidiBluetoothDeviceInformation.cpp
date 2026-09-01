#include "pch.h"
#include "MidiBluetoothDeviceInformation.h"
#include "Transports.Bluetooth.MidiBluetoothDeviceInformation.g.cpp"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothDeviceInformation::InternalInitializeFromJson(json::JsonObject const& deviceJson) noexcept
    {
        if (deviceJson == nullptr)
        {
            return;
        }

        try
        {
            m_bluetoothDeviceId = deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_ID_KEY, L"");
            m_bluetoothAddress = btinternal::BluetoothAddressFromDeviceId(m_bluetoothDeviceId);

            m_name = deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY, L"");

            m_selectedProtocol = btinternal::ProtocolFromJsonString(
                deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SELECTED_PROTOCOL_KEY, L""));

            m_isConnected = deviceJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_CONNECTED_KEY, false);
            m_isPaired = deviceJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY, false);
            m_isPresent = deviceJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PRESENT_KEY, false);

            m_signalStrengthDecibelMilliwatts = static_cast<int16_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_SIGNAL_STRENGTH_KEY, 0.0));

            m_lastSeenAgo = btinternal::TimeSpanFromMilliseconds(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEEN_AGO_MS_KEY, 0.0));

            m_hasBeenSeen = deviceJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_BEEN_SEEN_KEY, false);

            m_hasEndpoint = deviceJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_ENDPOINT_KEY, false);
            m_endpointDeviceId = deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_ID_KEY, L"");
            m_endpointDeviceInstanceId = deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ENDPOINT_DEVICE_INSTANCE_ID_KEY, L"");

            m_messagesReceived = static_cast<uint64_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_RECEIVED_KEY, 0.0));
            m_messagesSent = static_cast<uint64_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_MESSAGES_SENT_KEY, 0.0));

            m_packetsReceived = static_cast<uint64_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PACKETS_RECEIVED_KEY, 0.0));
            m_packetsSent = static_cast<uint64_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_PACKETS_SENT_KEY, 0.0));

            m_connectionInterval = btinternal::TimeSpanFromMilliseconds(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY, 0.0));

            m_lastConnectError = deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_KEY, L"");

            m_lastConnectErrorCode = static_cast<bluetooth::MidiBluetoothDeviceConnectErrorCode>(
                static_cast<uint32_t>(deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_CODE_KEY, 0.0)));

            m_lastConnectErrorHResult = static_cast<int32_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_CONNECT_ERROR_HRESULT_KEY, 0.0));
            m_lastSendErrorHResult = static_cast<int32_t>(
                deviceJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_LAST_SEND_ERROR_HRESULT_KEY, 0.0));

            m_offlineRetentionSeconds = btinternal::OfflineRetentionFromJsonString(
                deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_OFFLINE_RETENTION_KEY, L""));

            m_effectiveOfflineRetentionSeconds = btinternal::OfflineRetentionFromJsonString(
                deviceJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_EFFECTIVE_OFFLINE_RETENTION_KEY, L""));
        }
        catch (...)
        {
        }
    }
}
