// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once
#include "Utilities.Sequencing.MidiSequence.g.h"

#include "midi_file_sequence.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    // A handle over the sequence, not a copy of it. The sparse collections are built once when
    // first asked for and then kept; the dense parts are never built at all, and are read through
    // the fill methods a window at a time.
    struct MidiSequence : MidiSequenceT<MidiSequence>
    {
        MidiSequence() = default;

        Sequencing::MidiSequenceFormat Format() const noexcept;

        uint16_t TicksPerQuarterNote() const noexcept;
        bool UsesSmpteTiming() const noexcept;
        Sequencing::MidiSequenceTimingMode TimingMode() const noexcept;

        uint64_t DurationMicroseconds() const noexcept;
        uint32_t LastTick() const noexcept;

        uint32_t EventCount() const noexcept;
        uint32_t NoteCount() const noexcept;

        winrt::hstring Title() const noexcept { return m_title; }
        winrt::hstring Copyright() const noexcept { return m_copyright; }

        bool IsKaraoke() const noexcept;

        uint8_t LowestNoteNumber() const noexcept;
        uint8_t HighestNoteNumber() const noexcept;
        uint32_t LongestNoteTicks() const noexcept;

        uint16_t UsedChannelMask() const noexcept;

        foundation::Collections::IVectorView<Sequencing::MidiSequenceTrack> Tracks();
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTempoChange> TempoMap();
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTimeSignature> TimeSignatureMap();
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTextEvent> TextEvents();
        foundation::Collections::IVectorView<Sequencing::MidiSequenceLyricLine> LyricLines();

        uint64_t ConvertTickToMicroseconds(_In_ uint32_t const tick) const noexcept;
        uint32_t ConvertMicrosecondsToTick(_In_ uint64_t const microseconds) const noexcept;

        double GetBeatsPerMinuteAtTick(_In_ uint32_t const tick) const noexcept;
        Sequencing::MidiSequenceBarPosition GetBarPositionAtTick(_In_ uint32_t const tick) const noexcept;

        Sequencing::MidiSequenceTextEvent GetChordSymbolAtTick(_In_ uint32_t const tick);
        Sequencing::MidiSequenceLyricLine GetLyricLineAtTick(_In_ uint32_t const tick);

        uint32_t GetNoteCountInTickRange(_In_ uint32_t const startTick, _In_ uint32_t const endTick) const noexcept;

        uint32_t FillNotesInTickRange(
            _In_ uint32_t const startTick,
            _In_ uint32_t const endTick,
            _In_ uint32_t const startIndex,
            _Inout_ winrt::array_view<Sequencing::MidiSequenceNote> notes) const noexcept;

        uint32_t FillSoundingNoteCountsAtTick(
            _In_ uint32_t const tick,
            _Inout_ winrt::array_view<uint8_t> countsPerTrack) const noexcept;

        void InternalInitialize(_In_ std::shared_ptr<::midifile::MidiSequence const> const& sequence);

        // The player works against the same storage the projection wraps, so nothing about a
        // sequence has to cross the boundary in order to be played.
        std::shared_ptr<::midifile::MidiSequence const> InternalSequence() const noexcept { return m_sequence; }

    private:
        std::shared_ptr<::midifile::MidiSequence const> m_sequence{};

        winrt::hstring m_title{};
        winrt::hstring m_copyright{};

        foundation::Collections::IVectorView<Sequencing::MidiSequenceTrack> m_tracks{ nullptr };
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTempoChange> m_tempoMap{ nullptr };
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTimeSignature> m_timeSignatureMap{ nullptr };
        foundation::Collections::IVectorView<Sequencing::MidiSequenceTextEvent> m_textEvents{ nullptr };
        foundation::Collections::IVectorView<Sequencing::MidiSequenceLyricLine> m_lyricLines{ nullptr };
    };
}
