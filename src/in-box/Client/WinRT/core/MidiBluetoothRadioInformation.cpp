#include "pch.h"
#include "MidiBluetoothRadioInformation.h"
#include "Transports.Bluetooth.MidiBluetoothRadioInformation.g.cpp"

#include "midi_bluetooth_utility.h"

namespace winrt::Windows::Devices::Midi2::Transports::Bluetooth::implementation
{
    _Use_decl_annotations_
    void MidiBluetoothRadioInformation::InternalInitializeFromJson(json::JsonObject const& radioJson) noexcept
    {
        if (radioJson == nullptr)
        {
            return;
        }

        try
        {
            m_isPresent = radioJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PRESENT_KEY, false);
            m_isLowEnergySupported = radioJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_LOW_ENERGY_KEY, false);
            m_isCentralRoleSupported = radioJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_CENTRAL_ROLE_KEY, false);
            m_isPeripheralRoleSupported = radioJson.GetNamedBoolean(MIDI_CONFIG_JSON_BLUETOOTH_MIDI_RADIO_PERIPHERAL_ROLE_KEY, false);
        }
        catch (...)
        {
        }
    }
}
