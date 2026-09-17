// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of the Windows MIDI Services WinRT API and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiSequence.h"
#include "Utilities.Sequencing.MidiSequence.g.cpp"

#include "MidiSequenceTrack.h"
#include "MidiSequenceTextEvent.h"
#include "MidiSequenceLyricLine.h"

#include "wstring_util.h"

namespace winrt::Windows::Devices::Midi2::Utilities::Sequencing::implementation
{
    namespace
    {
        winrt::hstring FromUtf8(_In_ std::string const& value) noexcept
        {
            try
            {
                return winrt::hstring{ ::WindowsMidiServicesInternal::WStringFromUtf8(value) };
            }
            catch (...)
            {
                return {};
            }
        }

        Sequencing::MidiSequenceTextEvent MakeTextEvent(_In_ ::midifile::TextEvent const& source)
        {
            auto item = winrt::make_self<implementation::MidiSequenceTextEvent>();

            item->InternalInitialize(
                source.Tick,
                source.TrackIndex,
                static_cast<Sequencing::MidiSequenceTextKind>(source.Kind),
                FromUtf8(source.Text));

            return *item;
        }

        Sequencing::MidiSequenceLyricLine MakeLyricLine(_In_ ::midifile::LyricLine const& source)
        {
            auto item = winrt::make_self<implementation::MidiSequenceLyricLine>();

            item->InternalInitialize(source.StartTick, source.EndTick, FromUtf8(source.Text));

            return *item;
        }
    }

    _Use_decl_annotations_
    void MidiSequence::InternalInitialize(std::shared_ptr<::midifile::MidiSequence const> const& sequence)
    {
        m_sequence = sequence;

        if (m_sequence != nullptr)
        {
            m_title = FromUtf8(m_sequence->Title);
            m_copyright = FromUtf8(m_sequence->Copyright);
        }
    }

    Sequencing::MidiSequenceFormat MidiSequence::Format() const noexcept
    {
        return m_sequence == nullptr
            ? Sequencing::MidiSequenceFormat::MultiTrack
            : static_cast<Sequencing::MidiSequenceFormat>(m_sequence->Format);
    }

    uint16_t MidiSequence::TicksPerQuarterNote() const noexcept
    {
        return m_sequence == nullptr ? uint16_t{ 0 } : m_sequence->Division.TicksPerQuarterNote;
    }

    bool MidiSequence::UsesSmpteTiming() const noexcept
    {
        return m_sequence != nullptr && m_sequence->Division.IsSmpte;
    }

    uint64_t MidiSequence::DurationMicroseconds() const noexcept
    {
        return m_sequence == nullptr ? 0 : m_sequence->DurationMicroseconds;
    }

    uint32_t MidiSequence::LastTick() const noexcept
    {
        return m_sequence == nullptr ? 0 : m_sequence->LastTick;
    }

    uint32_t MidiSequence::EventCount() const noexcept
    {
        return m_sequence == nullptr ? 0 : static_cast<uint32_t>(m_sequence->Events.size());
    }

    uint32_t MidiSequence::NoteCount() const noexcept
    {
        return m_sequence == nullptr ? 0 : static_cast<uint32_t>(m_sequence->Notes.size());
    }

    bool MidiSequence::IsKaraoke() const noexcept
    {
        return m_sequence != nullptr && m_sequence->IsKaraoke;
    }

    uint8_t MidiSequence::LowestNoteNumber() const noexcept
    {
        return m_sequence == nullptr ? uint8_t{ 0 } : m_sequence->LowestNote;
    }

    uint8_t MidiSequence::HighestNoteNumber() const noexcept
    {
        return m_sequence == nullptr ? uint8_t{ 127 } : m_sequence->HighestNote;
    }

    uint32_t MidiSequence::LongestNoteTicks() const noexcept
    {
        return m_sequence == nullptr ? 0 : m_sequence->LongestNoteTicks;
    }

    uint16_t MidiSequence::UsedChannelMask() const noexcept
    {
        return m_sequence == nullptr ? uint16_t{ 0 } : m_sequence->UsedChannelMask;
    }

    foundation::Collections::IVectorView<Sequencing::MidiSequenceTrack> MidiSequence::Tracks()
    {
        if (m_tracks != nullptr)
        {
            return m_tracks;
        }

        std::vector<Sequencing::MidiSequenceTrack> items{};

        if (m_sequence != nullptr)
        {
            items.reserve(m_sequence->Tracks.size());

            for (size_t index = 0; index < m_sequence->Tracks.size(); ++index)
            {
                auto item = winrt::make_self<implementation::MidiSequenceTrack>();

                item->InternalInitialize(static_cast<uint16_t>(index), m_sequence->Tracks[index]);

                items.push_back(*item);
            }
        }

        m_tracks = winrt::single_threaded_vector<Sequencing::MidiSequenceTrack>(std::move(items)).GetView();

        return m_tracks;
    }

    foundation::Collections::IVectorView<Sequencing::MidiSequenceTempoChange> MidiSequence::TempoMap()
    {
        if (m_tempoMap != nullptr)
        {
            return m_tempoMap;
        }

        std::vector<Sequencing::MidiSequenceTempoChange> items{};

        if (m_sequence != nullptr)
        {
            items.reserve(m_sequence->TempoMap.size());

            for (auto const& entry : m_sequence->TempoMap)
            {
                items.push_back(Sequencing::MidiSequenceTempoChange{
                    entry.Tick,
                    entry.MicrosecondsPerQuarterNote,
                    entry.MicrosecondsPerQuarterNote == 0
                        ? 0.0
                        : 60000000.0 / static_cast<double>(entry.MicrosecondsPerQuarterNote),
                    entry.MicrosecondsAtTick });
            }
        }

        m_tempoMap = winrt::single_threaded_vector<Sequencing::MidiSequenceTempoChange>(std::move(items)).GetView();

        return m_tempoMap;
    }

    foundation::Collections::IVectorView<Sequencing::MidiSequenceTimeSignature> MidiSequence::TimeSignatureMap()
    {
        if (m_timeSignatureMap != nullptr)
        {
            return m_timeSignatureMap;
        }

        std::vector<Sequencing::MidiSequenceTimeSignature> items{};

        if (m_sequence != nullptr)
        {
            items.reserve(m_sequence->TimeSignatureMap.size());

            for (auto const& entry : m_sequence->TimeSignatureMap)
            {
                // The file stores the denominator as a power of two. A caller wants the number
                // it would see on a score.
                auto const denominator = entry.DenominatorPowerOfTwo >= 8
                    ? uint8_t{ 255 }
                    : static_cast<uint8_t>(1u << entry.DenominatorPowerOfTwo);

                items.push_back(Sequencing::MidiSequenceTimeSignature{
                    entry.Tick,
                    entry.Numerator,
                    denominator,
                    entry.TicksPerBar,
                    entry.BarNumberAtTick });
            }
        }

        m_timeSignatureMap = winrt::single_threaded_vector<Sequencing::MidiSequenceTimeSignature>(std::move(items)).GetView();

        return m_timeSignatureMap;
    }

    foundation::Collections::IVectorView<Sequencing::MidiSequenceTextEvent> MidiSequence::TextEvents()
    {
        if (m_textEvents != nullptr)
        {
            return m_textEvents;
        }

        std::vector<Sequencing::MidiSequenceTextEvent> items{};

        if (m_sequence != nullptr)
        {
            items.reserve(m_sequence->TextEvents.size());

            for (auto const& entry : m_sequence->TextEvents)
            {
                items.push_back(MakeTextEvent(entry));
            }
        }

        m_textEvents = winrt::single_threaded_vector<Sequencing::MidiSequenceTextEvent>(std::move(items)).GetView();

        return m_textEvents;
    }

    foundation::Collections::IVectorView<Sequencing::MidiSequenceLyricLine> MidiSequence::LyricLines()
    {
        if (m_lyricLines != nullptr)
        {
            return m_lyricLines;
        }

        std::vector<Sequencing::MidiSequenceLyricLine> items{};

        if (m_sequence != nullptr)
        {
            items.reserve(m_sequence->LyricLines.size());

            for (auto const& entry : m_sequence->LyricLines)
            {
                items.push_back(MakeLyricLine(entry));
            }
        }

        m_lyricLines = winrt::single_threaded_vector<Sequencing::MidiSequenceLyricLine>(std::move(items)).GetView();

        return m_lyricLines;
    }

    _Use_decl_annotations_
    uint64_t MidiSequence::ConvertTickToMicroseconds(uint32_t const tick) const noexcept
    {
        return m_sequence == nullptr ? 0 : m_sequence->MicrosecondsAtTick(tick);
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::ConvertMicrosecondsToTick(uint64_t const microseconds) const noexcept
    {
        return m_sequence == nullptr ? 0 : m_sequence->TickAtMicroseconds(microseconds);
    }

    _Use_decl_annotations_
    double MidiSequence::GetBeatsPerMinuteAtTick(uint32_t const tick) const noexcept
    {
        return m_sequence == nullptr ? 120.0 : m_sequence->BeatsPerMinuteAtTick(tick);
    }

    _Use_decl_annotations_
    Sequencing::MidiSequenceBarPosition MidiSequence::GetBarPositionAtTick(uint32_t const tick) const noexcept
    {
        if (m_sequence == nullptr)
        {
            return Sequencing::MidiSequenceBarPosition{ 1, 1, 0 };
        }

        auto const position = m_sequence->BarPositionAtTick(tick);

        return Sequencing::MidiSequenceBarPosition{ position.Bar, position.Beat, position.TicksIntoBeat };
    }

    _Use_decl_annotations_
    Sequencing::MidiSequenceTextEvent MidiSequence::GetChordSymbolAtTick(uint32_t const tick)
    {
        if (m_sequence == nullptr)
        {
            return nullptr;
        }

        auto const* const found = m_sequence->ChordSymbolAtTick(tick);

        return found == nullptr ? nullptr : MakeTextEvent(*found);
    }

    _Use_decl_annotations_
    Sequencing::MidiSequenceLyricLine MidiSequence::GetLyricLineAtTick(uint32_t const tick)
    {
        if (m_sequence == nullptr)
        {
            return nullptr;
        }

        auto const* const found = m_sequence->LyricLineAtTick(tick);

        return found == nullptr ? nullptr : MakeLyricLine(*found);
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::GetNoteCountInTickRange(uint32_t const startTick, uint32_t const endTick) const noexcept
    {
        if (m_sequence == nullptr || endTick < startTick)
        {
            return 0;
        }

        return m_sequence->CountNotesInTickRange(startTick, endTick);
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::FillNotesInTickRange(
        uint32_t const startTick,
        uint32_t const endTick,
        uint32_t const startIndex,
        winrt::array_view<Sequencing::MidiSequenceNote> notes) const noexcept
    {
        if (m_sequence == nullptr || endTick < startTick || startIndex >= notes.size())
        {
            return 0;
        }

        // The projected note and the one the reader built have the same shape, so this fills the
        // caller's array in place rather than building a list and copying it.
        auto written = uint32_t{ 0 };
        auto const capacity = static_cast<uint32_t>(notes.size()) - startIndex;

        m_sequence->ForEachNoteInTickRange(startTick, endTick, capacity,
            [&notes, &written, startIndex](::midifile::Note const& note) noexcept
            {
                notes[startIndex + written] = Sequencing::MidiSequenceNote{
                    note.StartTick,
                    note.EndTick,
                    note.TrackIndex,
                    note.Channel,
                    note.NoteNumber,
                    note.Velocity };

                ++written;
            });

        return written;
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::FillSoundingNoteCountsAtTick(
        uint32_t const tick,
        winrt::array_view<uint8_t> countsPerTrack) const noexcept
    {
        if (m_sequence == nullptr || countsPerTrack.size() == 0)
        {
            return 0;
        }

        return m_sequence->FillSoundingNoteCounts(tick, countsPerTrack.data(), countsPerTrack.size());
    }
}
