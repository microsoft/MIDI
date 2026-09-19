// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

// Builds Standard MIDI Files in memory so the reader can be tested against exact bytes, including
// shapes which are awkward to produce with a sequencer: running status, a split system exclusive
// dump, a chunk whose declared length lies.
namespace smftest
{
    class TrackBuilder
    {
    public:
        TrackBuilder& VariableLength(uint32_t value);
        TrackBuilder& Raw(std::initializer_list<uint8_t> bytes);
        TrackBuilder& RawBytes(std::vector<uint8_t> const& bytes);

        TrackBuilder& NoteOn(uint32_t delta, uint8_t channel, uint8_t note, uint8_t velocity);
        TrackBuilder& NoteOff(uint32_t delta, uint8_t channel, uint8_t note, uint8_t velocity);
        TrackBuilder& ControlChange(uint32_t delta, uint8_t channel, uint8_t controller, uint8_t value);
        TrackBuilder& ProgramChange(uint32_t delta, uint8_t channel, uint8_t program);

        // No status byte, so the reader has to carry the previous one forward.
        TrackBuilder& RunningStatusData(uint32_t delta, uint8_t data1, uint8_t data2);

        TrackBuilder& Tempo(uint32_t delta, uint32_t microsecondsPerQuarterNote);
        TrackBuilder& TimeSignature(uint32_t delta, uint8_t numerator, uint8_t denominatorPowerOfTwo);
        TrackBuilder& MetaText(uint32_t delta, uint8_t metaType, std::string const& text);

        TrackBuilder& SystemExclusive(uint32_t delta, std::vector<uint8_t> const& payload);
        TrackBuilder& Escape(uint32_t delta, std::vector<uint8_t> const& payload);

        TrackBuilder& EndOfTrack(uint32_t delta = 0);

        std::vector<uint8_t> const& Bytes() const noexcept { return m_bytes; }

    private:
        std::vector<uint8_t> m_bytes{};
    };

    std::vector<uint8_t> BuildFile(
        uint16_t format,
        uint16_t division,
        std::vector<TrackBuilder> const& tracks);

    // Lets a test declare a track length which does not match the bytes present.
    std::vector<uint8_t> BuildFileWithTrackLength(
        uint16_t format,
        uint16_t division,
        TrackBuilder const& track,
        uint32_t declaredLength);

    std::vector<uint8_t> WrapInRiff(std::vector<uint8_t> const& midiFile);
}
