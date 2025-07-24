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
        MidiClockGenerator() = default;

        MidiClockGenerator(_In_ collections::IVector<midi2::MidiEndpointConnection> const& connections) noexcept;
        MidiClockGenerator(_In_ collections::IVector<midi2::MidiEndpointConnection> const& connections, _In_ double const bpm) noexcept;
        MidiClockGenerator(_In_ collections::IVector<midi2::MidiEndpointConnection> const& connections, _In_ double const bpm, _In_ uint16_t const pulsesPerQuarterNote) noexcept;
        
        void UpdateRunningClock(_In_ double const bpm) noexcept;
        void UpdateRunningClock(_In_ double const bpm, _In_ uint16_t const pulsesPerQuarterNote) noexcept;
        
        bool Start(_In_ bool const sendMidiStartMessage) noexcept;
        bool Stop(_In_ bool const sendMidiStopMessage) noexcept;

    private:
        uint16_t m_pulsesPerQuarterNote{ 24 };
        double m_bpm{ 120 };

        collections::IVector<midi2::MidiEndpointConnection> m_connections;

        std::jthread m_workerThread;

        void WorkerThread();

    };
}
namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiClockGenerator : MidiClockGeneratorT<MidiClockGenerator, implementation::MidiClockGenerator>
    {
    };
}
