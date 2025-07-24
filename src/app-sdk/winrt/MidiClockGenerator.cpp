// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services App SDK and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiClockGenerator.h"
#include "Utilities.Sequencing.MidiClockGenerator.g.cpp"


namespace winrt::Microsoft::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    _Use_decl_annotations_
    MidiClockGenerator::MidiClockGenerator(
        collections::IVector<midi2::MidiEndpointConnection> const& connections) noexcept
    {
        UNREFERENCED_PARAMETER(connections);
    }

    _Use_decl_annotations_
    MidiClockGenerator::MidiClockGenerator(
        collections::IVector<midi2::MidiEndpointConnection> const& connections,
        double const bpm) noexcept
    {
        UNREFERENCED_PARAMETER(connections);
        UNREFERENCED_PARAMETER(bpm);
    }

    _Use_decl_annotations_
    MidiClockGenerator::MidiClockGenerator(
        collections::IVector<midi2::MidiEndpointConnection> const& connections,
        double bpm, 
        uint16_t const pulsesPerQuarterNote) noexcept
    {
        UNREFERENCED_PARAMETER(connections);
        UNREFERENCED_PARAMETER(bpm);
        UNREFERENCED_PARAMETER(pulsesPerQuarterNote);
    }

    _Use_decl_annotations_
    void MidiClockGenerator::UpdateRunningClock(
        double const bpm) noexcept
    {
        UNREFERENCED_PARAMETER(bpm);
    }

    _Use_decl_annotations_
    void MidiClockGenerator::UpdateRunningClock(
        double const bpm, 
        uint16_t const pulsesPerQuarterNote) noexcept
    {
        UNREFERENCED_PARAMETER(bpm);
        UNREFERENCED_PARAMETER(pulsesPerQuarterNote);
    }

    _Use_decl_annotations_
    bool MidiClockGenerator::Start(
        bool const sendMidiStartMessage) noexcept
    {
        UNREFERENCED_PARAMETER(sendMidiStartMessage);

        return false;
    }

    _Use_decl_annotations_
    bool MidiClockGenerator::Stop(
        bool const sendMidiStopMessage) noexcept
    {
        UNREFERENCED_PARAMETER(sendMidiStopMessage);

        return false;
    }
}
