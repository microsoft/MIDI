// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequenceBuilder.h"
#include "Utilities.Sequencing.MidiSequenceBuilder.g.cpp"

#include "MidiSequence.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    namespace
    {
        constexpr uint8_t StatusNoteOff = 0x80;
        constexpr uint8_t StatusNoteOn = 0x90;

        constexpr uint32_t MicrosecondsPerMinute = 60000000;

        // Everything a caller supplies is untrusted. A tick past the end, a track index that was
        // never created or a word count of zero has to be refused rather than trusted, because a
        // sequence is handed straight to the playback engine afterwards.
        constexpr size_t MaximumEvents = 2000000;
        constexpr size_t MaximumTracks = 1000;
        constexpr size_t MaximumSystemExclusiveBytes = 1 << 20;
        constexpr uint32_t MaximumWordsPerMessage = 1024;
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::TicksPerQuarterNote(uint16_t const value) noexcept
    {
        // Zero would make every musical time conversion a division by zero further down.
        m_ticksPerQuarterNote = value == 0 ? ::midifile::DefaultTicksPerQuarterNote : value;
    }

    _Use_decl_annotations_
    uint16_t MidiSequenceBuilder::AddTrack(winrt::hstring const& name)
    {
        if (m_tracks.size() >= MaximumTracks)
        {
            return static_cast<uint16_t>(m_tracks.size() - 1);
        }

        ::midifile::Track track{};

        track.Name = winrt::to_string(name);

        m_tracks.push_back(std::move(track));

        return static_cast<uint16_t>(m_tracks.size() - 1);
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AddTempoChange(uint32_t const tick, double const beatsPerMinute)
    {
        if (!(beatsPerMinute > 0.0) || beatsPerMinute > 10000.0)
        {
            return;
        }

        ::midifile::TempoChange change{};

        change.Tick = tick;
        change.MicrosecondsPerQuarterNote = static_cast<uint32_t>(
            static_cast<double>(MicrosecondsPerMinute) / beatsPerMinute);

        m_tempoMap.push_back(change);
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AddTimeSignature(uint32_t const tick, uint8_t const numerator, uint8_t const denominator)
    {
        // A denominator is written as a power of two in a file, and only powers of two are legal.
        if (numerator == 0 || denominator == 0 || (denominator & (denominator - 1)) != 0)
        {
            return;
        }

        uint8_t power = 0;

        for (uint8_t value = denominator; value > 1; value >>= 1)
        {
            power++;
        }

        ::midifile::TimeSignatureChange change{};

        change.Tick = tick;
        change.Numerator = numerator;
        change.DenominatorPowerOfTwo = power;

        m_timeSignatureMap.push_back(change);
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AppendEvent(
        uint16_t const trackIndex,
        uint32_t const tick,
        ::midifile::EventKind const kind,
        uint8_t const channel,
        std::span<uint8_t const> const bytes)
    {
        if (m_events.size() >= MaximumEvents || bytes.empty() || !IsKnownTrack(trackIndex))
        {
            return;
        }

        ::midifile::SequenceEvent event{};

        event.Tick = tick;
        event.TrackIndex = trackIndex;
        event.Kind = kind;
        event.Channel = channel;
        event.ByteOffset = static_cast<uint32_t>(m_eventBytes.size());
        event.ByteCount = static_cast<uint32_t>(bytes.size());

        m_eventBytes.insert(m_eventBytes.end(), bytes.begin(), bytes.end());
        m_events.push_back(event);
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AddMessages(uint16_t const trackIndex, uint32_t const tick, array_view<uint32_t const> words)
    {
        if (words.size() == 0 || words.size() > MaximumWordsPerMessage)
        {
            return;
        }

        // Stored in native byte order, which is what the engine reads them back as.
        std::vector<uint8_t> bytes(words.size() * sizeof(uint32_t));

        std::memcpy(bytes.data(), words.data(), bytes.size());

        AppendEvent(
            trackIndex,
            tick,
            ::midifile::EventKind::UniversalPacket,
            ::midifile::ChannelNone,
            std::span<uint8_t const>{ bytes.data(), bytes.size() });
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AddSystemExclusive(uint16_t const trackIndex, uint32_t const tick, array_view<uint8_t const> data)
    {
        if (data.size() < 2 || data.size() > MaximumSystemExclusiveBytes)
        {
            return;
        }

        // The stored form always carries its own status byte, which is what lets the engine
        // convert it without tracking running status.
        if (data[0] != 0xF0 || data[data.size() - 1] != 0xF7)
        {
            return;
        }

        AppendEvent(
            trackIndex,
            tick,
            ::midifile::EventKind::SystemExclusive,
            ::midifile::ChannelNone,
            std::span<uint8_t const>{ data.data(), data.size() });
    }

    _Use_decl_annotations_
    void MidiSequenceBuilder::AddNote(
        uint32_t const tick,
        uint32_t const durationTicks,
        uint16_t const trackIndex,
        midi2::MidiChannel const& channel,
        uint8_t const noteNumber,
        uint8_t const velocity)
    {
        if (channel == nullptr || noteNumber > 127)
        {
            return;
        }

        auto const channelIndex = static_cast<uint8_t>(channel.Index() & 0x0F);

        // A zero length note would pair with itself and sound for no time at all, so it gets the
        // shortest length that still has two distinct ticks.
        auto const length = durationTicks == 0 ? 1u : durationTicks;

        // Clamped rather than wrapped: a note that ends before it starts would never be paired
        // and would hang.
        auto const endTick = (tick > 0xFFFFFFFFu - length) ? 0xFFFFFFFFu : tick + length;

        std::array<uint8_t, 3> noteOn{
            static_cast<uint8_t>(StatusNoteOn | channelIndex),
            static_cast<uint8_t>(noteNumber & 0x7F),
            static_cast<uint8_t>(velocity & 0x7F) };

        std::array<uint8_t, 3> noteOff{
            static_cast<uint8_t>(StatusNoteOff | channelIndex),
            static_cast<uint8_t>(noteNumber & 0x7F),
            uint8_t{ 0 } };

        AppendEvent(trackIndex, tick, ::midifile::EventKind::NoteOn, channelIndex,
            std::span<uint8_t const>{ noteOn.data(), noteOn.size() });

        AppendEvent(trackIndex, endTick, ::midifile::EventKind::NoteOff, channelIndex,
            std::span<uint8_t const>{ noteOff.data(), noteOff.size() });
    }

    Sequencing::MidiSequence MidiSequenceBuilder::GetSequence()
    {
        auto sequence = std::make_shared<::midifile::MidiSequence>();

        sequence->Format = ::midifile::SequenceFormat::MultiTrack;
        sequence->Division.IsSmpte = false;
        sequence->Division.TicksPerQuarterNote = m_ticksPerQuarterNote;
        sequence->Timing = m_timingMode == Sequencing::MidiSequenceTimingMode::Absolute
            ? ::midifile::TimingMode::Absolute
            : ::midifile::TimingMode::Musical;

        sequence->Tracks = m_tracks;
        sequence->Events = m_events;
        sequence->EventBytes = m_eventBytes;
        sequence->TempoMap = m_tempoMap;
        sequence->TimeSignatureMap = m_timeSignatureMap;

        // Builds the derived maps, the totals and the note pairing, exactly as it does for a file.
        sequence->Finalize();

        auto projected = winrt::make_self<Sequencing::implementation::MidiSequence>();

        projected->InternalInitialize(sequence);

        return *projected;
    }

    void MidiSequenceBuilder::Clear() noexcept
    {
        m_tracks.clear();
        m_events.clear();
        m_eventBytes.clear();
        m_tempoMap.clear();
        m_timeSignatureMap.clear();
    }
}
