// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "RecordingTake.h"

#include "MessageTranslation.h"

#include <algorithm>

namespace midisequencer
{
    namespace
    {
        // Far above anything a person plays, so a stuck controller can't fill memory.
        constexpr size_t MaximumTakeItems = 4000000;

        uint8_t MessageType(_In_ uint32_t word0) noexcept { return static_cast<uint8_t>(word0 >> 28); }
        uint8_t GroupOf(_In_ uint32_t word0) noexcept { return static_cast<uint8_t>((word0 >> 24) & 0x0F); }
        uint8_t StatusOf(_In_ uint32_t word0) noexcept { return static_cast<uint8_t>((word0 >> 20) & 0x0F); }
        uint8_t ChannelOf(_In_ uint32_t word0) noexcept { return static_cast<uint8_t>((word0 >> 16) & 0x0F); }
    }

    _Use_decl_annotations_
    bool PassesRecordFilter(RecordFilter const& filter, uint32_t const* words, uint8_t wordCount) noexcept
    {
        if (wordCount == 0)
        {
            return false;
        }

        auto const type = MessageType(words[0]);

        // Utility, stream and real time messages (clock, start, stop) are never recorded.
        if (type == 0x0 || type == 0xF || type == 0x1)
        {
            return false;
        }

        if (filter.Group >= 0 && GroupOf(words[0]) != static_cast<uint8_t>(filter.Group))
        {
            return false;
        }

        if (type == 0x3 || type == 0x5)
        {
            return filter.SystemExclusive;
        }

        if (type != 0x2 && type != 0x4)
        {
            // Flex Data and anything newer: not recorded yet.
            return false;
        }

        if ((filter.Channels & (1u << ChannelOf(words[0]))) == 0)
        {
            return false;
        }

        switch (StatusOf(words[0]))
        {
        case 0x8:
        case 0x9:
            return filter.Notes;
        case 0xA:       // poly pressure
        case 0xD:       // channel pressure
            return filter.Pressure;
        case 0xB:
            return filter.Controllers;
        case 0xC:
            return filter.Program;
        case 0xE:
            return filter.PitchBend;
        case 0x0:       // registered per-note controller
        case 0x1:       // assignable per-note controller
        case 0x2:       // registered controller (RPN)
        case 0x3:       // assignable controller (NRPN)
        case 0x4:       // relative registered controller
        case 0x5:       // relative assignable controller
            return filter.Controllers;
        case 0x6:       // per-note pitch bend
            return filter.PitchBend;
        case 0xF:       // per-note management
            return filter.Notes;
        default:
            return false;
        }
    }

    _Use_decl_annotations_
    RecordingTake::RecordingTake(int64_t startTick, RecordFilter filter) :
        m_startTick(startTick), m_filter(filter), m_lastTick(startTick)
    {
    }

    _Use_decl_annotations_
    void RecordingTake::Add(int64_t tick, uint32_t const* words, uint8_t wordCount)
    {
        if (wordCount == 0 || !PassesRecordFilter(m_filter, words, wordCount))
        {
            return;
        }

        if (m_notes.size() + m_events.size() + m_open.size() >= MaximumTakeItems)
        {
            return;
        }

        // A message that arrives a little before the take started still belongs to it.
        tick = std::max(tick, m_startTick);
        m_lastTick = std::max(m_lastTick, tick);

        auto const translated = TranslateToMidi2(words, wordCount);

        for (uint8_t i = 0; i < translated.Count; ++i)
        {
            auto const& message = translated.Messages[i];
            auto const count = translated.WordCounts[i];
            auto const word0 = message[0];

            if (MessageType(word0) == 0x4 && (StatusOf(word0) == 0x9 || StatusOf(word0) == 0x8))
            {
                auto const channel = ChannelOf(word0);
                auto const number = static_cast<uint8_t>((word0 >> 8) & 0x7F);

                // Any note on this channel and number that's still held ends here: a second note on
                // without an off in between restarts the note.
                auto open = std::find_if(m_open.begin(), m_open.end(), [&](OpenNote const& note)
                {
                    return note.Value.Channel == channel && note.Value.Number == number;
                });

                if (open != m_open.end())
                {
                    auto note = open->Value;
                    note.Length = std::max<int64_t>(1, (tick - m_startTick) - note.Tick);

                    if (StatusOf(word0) == 0x8)
                    {
                        note.ReleaseVelocity = static_cast<uint16_t>(message[1] >> 16);
                    }

                    m_notes.push_back(note);
                    m_open.erase(open);
                }

                if (StatusOf(word0) == 0x9)
                {
                    OpenNote note{};
                    note.Value.Tick = tick - m_startTick;
                    note.Value.Channel = channel;
                    note.Value.Number = number;
                    note.Value.Velocity = static_cast<uint16_t>(message[1] >> 16);
                    note.Value.AttributeType = static_cast<uint8_t>(word0 & 0xFF);
                    note.Value.AttributeData = static_cast<uint16_t>(message[1] & 0xFFFF);
                    m_open.push_back(note);
                }

                continue;
            }

            ClipEvent event{};
            event.Tick = tick - m_startTick;
            event.WordCount = std::min<uint8_t>(count, 4);
            std::copy_n(message.begin(), event.WordCount, event.Words.begin());
            m_events.push_back(event);
        }
    }

    _Use_decl_annotations_
    Clip RecordingTake::Finish(
        int64_t endTick,
        std::vector<MeterChange> const& meter,
        std::wstring clipId,
        std::wstring name,
        std::wstring originDetail)
    {
        auto const end = std::max(endTick, m_lastTick);

        for (auto const& open : m_open)
        {
            auto note = open.Value;
            note.Length = std::max<int64_t>(1, (end - m_startTick) - note.Tick);
            m_notes.push_back(note);
        }

        m_open.clear();

        Clip clip{};
        clip.Id = std::move(clipId);
        clip.Kind = ClipKind::Notes;
        clip.Name = std::move(name);
        clip.Loop = false;
        clip.Origin = ClipOrigin::Recorded;
        clip.OriginDetail = std::move(originDetail);
        clip.Notes = std::move(m_notes);
        clip.Events = std::move(m_events);

        // From the start of the bar the take began in to the end of the bar it ended in, so a
        // recorded clip lines up with the bars.
        auto const barStart = TickAtBar(meter, BarPositionAtTick(meter, m_startTick).Bar);
        auto const shift = m_startTick - barStart;
        m_placementTick = barStart;

        for (auto& note : clip.Notes)
        {
            note.Tick += shift;
        }

        for (auto& event : clip.Events)
        {
            event.Tick += shift;
        }

        int64_t last = end - barStart;

        for (auto const& note : clip.Notes)
        {
            last = std::max(last, note.Tick + note.Length);
        }

        auto const absoluteEnd = barStart + std::max<int64_t>(1, last);
        auto const position = BarPositionAtTick(meter, absoluteEnd - 1);
        auto const barEnd = TickAtBar(meter, position.Bar + 1);

        clip.Length = std::max<int64_t>(1, barEnd - barStart);

        SortClip(clip);

        m_notes.clear();
        m_events.clear();

        return clip;
    }

    _Use_decl_annotations_
    std::vector<Note> RecordingTake::NotesSoFar(int64_t nowTick) const
    {
        std::vector<Note> notes = m_notes;

        for (auto const& open : m_open)
        {
            auto note = open.Value;
            note.Length = std::max<int64_t>(1, (nowTick - m_startTick) - note.Tick);
            notes.push_back(note);
        }

        return notes;
    }
}
