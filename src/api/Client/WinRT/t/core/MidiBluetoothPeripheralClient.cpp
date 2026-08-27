#include "pch.h"
#include "MidiBluetoothPeripheralClient.h"
#include "Transports.Bluetooth.MidiBluetoothPeripheralClient.g.cpp"

#include "midi_bluetooth_utility.h"

namespace btinternal = ::Windows::Devices::Midi2::Transports::Bluetooth::Internal;

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothPeripheralClient::InternalInitializeFromJson(json::JsonObject const& peripheralJson) noexcept
    {
        if (peripheralJson == nullptr)
        {
            return;
        }

        try
        {
            m_name = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_DEVICE_NAME_KEY, L"");
            m_hasGenericName = peripheralJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_HAS_GENERIC_NAME_KEY, false);

            m_bluetoothAddress = btinternal::BluetoothAddressFromDeviceId(
                peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_KEY, L""));

            m_bluetoothAddressType = btinternal::AddressTypeFromJsonString(
                peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_ADDRESS_TYPE_KEY, L""));

            m_isPaired = peripheralJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_IS_PAIRED_KEY, false);

            m_windowsDeviceId = peripheralJson.GetNamedString(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_BLUETOOTH_DEVICE_ID_KEY, L"");

            m_connectionInterval = btinternal::TimeSpanFromMilliseconds(
                peripheralJson.GetNamedNumber(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_INTERVAL_MS_KEY, 0.0));
        }
        catch (...)
        {
        }
    }
}
