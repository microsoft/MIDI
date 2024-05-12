#pragma once
#include "MidiStreamConfigurationRequestedSettings.g.h"

#define MIDI_DEFAULT_SPEC_VERSION_MAJOR 1
#define MIDI_DEFAULT_SPEC_VERSION_MINOR 1

#define MIDI_DEFAULT_JR_REQUEST_TRANSMIT false
#define MIDI_DEFAULT_JR_REQUEST_RECEIVE false

#define MIDI_DEFAULT_MIDI_PROTOCOL midi2::MidiProtocol::Midi2

namespace winrt::Microsoft::Devices::Midi2::implementation
{
    struct MidiStreamConfigurationRequestedSettings : MidiStreamConfigurationRequestedSettingsT<MidiStreamConfigurationRequestedSettings>
    {
        MidiStreamConfigurationRequestedSettings() = default;

        uint8_t SpecificationVersionMajor() const noexcept { return m_specVersionMajor; }
        void SpecificationVersionMajor(_In_ uint8_t const value) noexcept { m_specVersionMajor = value; }

        uint8_t SpecificationVersionMinor() const noexcept { return m_specVersionMinor; }
        void SpecificationVersionMinor(_In_ uint8_t const value) noexcept { m_specVersionMinor = value; }

        midi2::MidiProtocol PreferredMidiProtocol() const noexcept { return m_midiProtocol; }
        void PreferredMidiProtocol(_In_ midi2::MidiProtocol const& value) noexcept { m_midiProtocol = value; }

        bool RequestEndpointTransmitJitterReductionTimestamps() const noexcept { return m_requestEndpointTransmitJitterReductionTimestamps; }
        void RequestEndpointTransmitJitterReductionTimestamps(_In_ bool const value) noexcept { m_requestEndpointTransmitJitterReductionTimestamps = value; }

        bool RequestEndpointReceiveJitterReductionTimestamps() const noexcept { return m_requestEndpointReceiveJitterReductionTimestamps; }
        void RequestEndpointReceiveJitterReductionTimestamps(_In_ bool const value) noexcept { m_requestEndpointReceiveJitterReductionTimestamps = value; }

    private:
        uint8_t m_specVersionMajor{ MIDI_DEFAULT_SPEC_VERSION_MAJOR };
        uint8_t m_specVersionMinor{ MIDI_DEFAULT_SPEC_VERSION_MINOR };

        midi2::MidiProtocol m_midiProtocol { MIDI_DEFAULT_MIDI_PROTOCOL };

        bool m_requestEndpointTransmitJitterReductionTimestamps{ MIDI_DEFAULT_JR_REQUEST_TRANSMIT };
        bool m_requestEndpointReceiveJitterReductionTimestamps{ MIDI_DEFAULT_JR_REQUEST_RECEIVE };

    };
}
namespace winrt::Microsoft::Devices::Midi2::factory_implementation
{
    struct MidiStreamConfigurationRequestedSettings : MidiStreamConfigurationRequestedSettingsT<MidiStreamConfigurationRequestedSettings, implementation::MidiStreamConfigurationRequestedSettings>
    {
    };
}
