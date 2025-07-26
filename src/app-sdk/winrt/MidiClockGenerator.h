// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiClockGenerator.g.h"

namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    struct MidiClockGenerator : MidiClockGeneratorT<MidiClockGenerator>
    {
        MidiClockGenerator(_In_ collections::IVector<midi2::Utilities::Sequencing::MidiClockDestination> const& destinations, _In_ double const bpm, _In_ uint16_t const pulsesPerQuarterNote) noexcept;       

        collections::IVectorView<midi2::Utilities::Sequencing::MidiClockDestination> Destinations() { return m_destinations.GetView(); }

        void UpdateRunningClock(_In_ double const bpm) noexcept;
        void UpdateRunningClock(_In_ double const bpm, _In_ uint16_t const pulsesPerQuarterNote) noexcept;
        
        bool Start(_In_ bool const sendMidiStartMessage) noexcept;
        bool Stop(_In_ bool const sendMidiStopMessage) noexcept;

    private:
        uint16_t m_pulsesPerQuarterNote{ 24 };
        double m_bpm{ 120 };

        uint64_t m_midiTimestampClockTicksPerMinute{ 0 };

        bool m_sendMidiStartMessage{ false };
        bool m_sendMidiStopMessage{ false };

        collections::IVector<midi2::Utilities::Sequencing::MidiClockDestination> m_destinations = 
            winrt::single_threaded_vector<midi2::Utilities::Sequencing::MidiClockDestination>();

        std::jthread m_workerThread;
        std::stop_token m_workerThreadStopToken;

        const uint32_t m_umpMidiTimingClockMessage{ 0x10F80000 };   // does not include group index
        const uint32_t m_umpMidiStartMessage{ 0x10FA0000 };         // does not include group index
        const uint32_t m_umpMidiStopMessage{ 0x10FC0000 };          // does not include group index

        void ThreadWorker();
        void SendMessageToAllDestinations(_In_ internal::MidiTimestamp timestamp, _In_ uint32_t const message);

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiClockGenerator : MidiClockGeneratorT<MidiClockGenerator, implementation::MidiClockGenerator>
    {
    };
}
