// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "Utilities.Sequencing.MidiSequenceBuilder.g.h"

#include "midi_file_sequence.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    // Fills a native sequence as the caller goes, and hands out a finished copy on demand. The
    // derived maps and the note pairing are built by the model's own Finalize, so this only ever
    // has to write the raw parts.
    struct MidiSequenceBuilder : MidiSequenceBuilderT<MidiSequenceBuilder>
    {
        MidiSequenceBuilder() = default;

        uint16_t TicksPerQuarterNote() const noexcept { return m_ticksPerQuarterNote; }
        void TicksPerQuarterNote(_In_ uint16_t const value) noexcept;

        Sequencing::MidiSequenceTimingMode TimingMode() const noexcept { return m_timingMode; }
        void TimingMode(_In_ Sequencing::MidiSequenceTimingMode const value) noexcept { m_timingMode = value; }

        uint16_t AddTrack(_In_ winrt::hstring const& name);

        void AddTempoChange(_In_ uint32_t const tick, _In_ double const beatsPerMinute);
        void AddTimeSignature(_In_ uint32_t const tick, _In_ uint8_t const numerator, _In_ uint8_t const denominator);

        void AddMessages(_In_ uint16_t const trackIndex, _In_ uint32_t const tick, _In_ array_view<uint32_t const> words);
        void AddSystemExclusive(_In_ uint16_t const trackIndex, _In_ uint32_t const tick, _In_ array_view<uint8_t const> data);

        void AddNote(
            _In_ uint32_t const tick,
            _In_ uint32_t const durationTicks,
            _In_ uint16_t const trackIndex,
            _In_ midi2::MidiChannel const& channel,
            _In_ uint8_t const noteNumber,
            _In_ uint8_t const velocity);

        Sequencing::MidiSequence GetSequence();

        void Clear() noexcept;

    private:
        void AppendEvent(
            _In_ uint16_t const trackIndex,
            _In_ uint32_t const tick,
            _In_ ::midifile::EventKind const kind,
            _In_ uint8_t const channel,
            _In_ std::span<uint8_t const> const bytes);

        bool IsKnownTrack(_In_ uint16_t const trackIndex) const noexcept
        {
            return trackIndex < m_tracks.size();
        }

        uint16_t m_ticksPerQuarterNote{ ::midifile::DefaultTicksPerQuarterNote };
        Sequencing::MidiSequenceTimingMode m_timingMode{ Sequencing::MidiSequenceTimingMode::Musical };

        std::vector<::midifile::Track> m_tracks{};
        std::vector<::midifile::SequenceEvent> m_events{};
        std::vector<uint8_t> m_eventBytes{};
        std::vector<::midifile::TempoChange> m_tempoMap{};
        std::vector<::midifile::TimeSignatureChange> m_timeSignatureMap{};
    };
}

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::factory_implementation
{
    struct MidiSequenceBuilder : MidiSequenceBuilderT<MidiSequenceBuilder, implementation::MidiSequenceBuilder>
    {
    };
}
