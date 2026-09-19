// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Deliberately free of pch.h, WinRT and XAML so that this file compiles unchanged into the unit
// test project. See MidiSequence.h.

#include "midi_file_sequence.h"

#include <algorithm>
#include <map>
#include <utility>

namespace midifile
{
    namespace
    {
        constexpr uint64_t MicrosecondsPerSecond = 1000000;

        uint32_t NominalTicksPerQuarterNote(TimeDivision const& division) noexcept
        {
            if (!division.IsSmpte)
            {
                return division.TicksPerQuarterNote == 0
                    ? DefaultTicksPerQuarterNote
                    : division.TicksPerQuarterNote;
            }

            // A real time division has no musical quarter note. Counting bars against a nominal
            // 120 beats per minute at least keeps a bar display moving rather than dividing by zero.
            auto const perSecond = division.TicksPerSecond();

            return perSecond > 0.0 ? static_cast<uint32_t>(perSecond / 2.0) : DefaultTicksPerQuarterNote;
        }

        uint32_t ComputeTicksPerBar(TimeSignatureChange const& signature, uint32_t ticksPerQuarterNote) noexcept
        {
            auto const numerator = signature.Numerator == 0 ? 4u : static_cast<uint32_t>(signature.Numerator);

            // A denominator of 2^n where n can be large is legal in the file and meaningless in
            // music; clamping keeps the shift defined and the result non zero.
            auto const power = signature.DenominatorPowerOfTwo > 16 ? uint8_t{ 16 } : signature.DenominatorPowerOfTwo;

            auto const wholeNoteTicks = static_cast<uint64_t>(ticksPerQuarterNote) * 4;
            auto const beatTicks = wholeNoteTicks >> power;

            auto const barTicks = beatTicks * numerator;

            if (barTicks == 0 || barTicks > 0xFFFFFFFFull)
            {
                return ticksPerQuarterNote * 4;
            }

            return static_cast<uint32_t>(barTicks);
        }
    }

    double TimeDivision::TicksPerSecond() const noexcept
    {
        if (!IsSmpte)
        {
            return 0.0;
        }

        // 29 in the file means 29.97 drop frame, the only fractional rate SMPTE defines.
        double const framesPerSecond = FramesPerSecond == 29 ? (30000.0 / 1001.0) : static_cast<double>(FramesPerSecond);

        return framesPerSecond * static_cast<double>(TicksPerFrame);
    }

    _Use_decl_annotations_
    std::span<uint8_t const> MidiSequence::BytesOf(SequenceEvent const& event) const noexcept
    {
        if (event.ByteCount == 0 || event.ByteOffset > EventBytes.size())
        {
            return {};
        }

        auto const available = EventBytes.size() - event.ByteOffset;
        auto const count = static_cast<size_t>(event.ByteCount) > available
            ? available
            : static_cast<size_t>(event.ByteCount);

        return std::span<uint8_t const>{ EventBytes.data() + event.ByteOffset, count };
    }

    uint64_t MidiSequence::MicrosecondsAtTick(uint32_t tick) const noexcept
    {
        if (Division.IsSmpte)
        {
            auto const perSecond = Division.TicksPerSecond();

            if (perSecond <= 0.0)
            {
                return 0;
            }

            return static_cast<uint64_t>((static_cast<double>(tick) / perSecond) * static_cast<double>(MicrosecondsPerSecond));
        }

        if (TempoMap.empty())
        {
            return static_cast<uint64_t>(tick) * DefaultMicrosecondsPerQuarterNote / NominalTicksPerQuarterNote(Division);
        }

        auto const found = std::upper_bound(
            TempoMap.begin(),
            TempoMap.end(),
            tick,
            [](uint32_t value, TempoChange const& entry) noexcept { return value < entry.Tick; });

        auto const& entry = *(found == TempoMap.begin() ? found : std::prev(found));

        if (tick <= entry.Tick)
        {
            return entry.MicrosecondsAtTick;
        }

        auto const elapsed = static_cast<uint64_t>(tick - entry.Tick) * entry.MicrosecondsPerQuarterNote
            / NominalTicksPerQuarterNote(Division);

        return entry.MicrosecondsAtTick + elapsed;
    }

    uint32_t MidiSequence::TickAtMicroseconds(uint64_t microseconds) const noexcept
    {
        if (Division.IsSmpte)
        {
            auto const perSecond = Division.TicksPerSecond();

            if (perSecond <= 0.0)
            {
                return 0;
            }

            auto const ticks = (static_cast<double>(microseconds) / static_cast<double>(MicrosecondsPerSecond)) * perSecond;

            return ticks >= 4294967295.0 ? 0xFFFFFFFFu : static_cast<uint32_t>(ticks);
        }

        auto const ticksPerQuarterNote = NominalTicksPerQuarterNote(Division);

        if (TempoMap.empty())
        {
            auto const ticks = microseconds * ticksPerQuarterNote / DefaultMicrosecondsPerQuarterNote;
            return ticks >= 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(ticks);
        }

        auto const found = std::upper_bound(
            TempoMap.begin(),
            TempoMap.end(),
            microseconds,
            [](uint64_t value, TempoChange const& entry) noexcept { return value < entry.MicrosecondsAtTick; });

        auto const& entry = *(found == TempoMap.begin() ? found : std::prev(found));

        if (microseconds <= entry.MicrosecondsAtTick || entry.MicrosecondsPerQuarterNote == 0)
        {
            return entry.Tick;
        }

        auto const elapsed = (microseconds - entry.MicrosecondsAtTick) * ticksPerQuarterNote
            / entry.MicrosecondsPerQuarterNote;

        auto const tick = static_cast<uint64_t>(entry.Tick) + elapsed;

        return tick >= 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(tick);
    }

    double MidiSequence::BeatsPerMinuteAtTick(uint32_t tick) const noexcept
    {
        if (Division.IsSmpte || TempoMap.empty())
        {
            return 60000000.0 / static_cast<double>(DefaultMicrosecondsPerQuarterNote);
        }

        auto const found = std::upper_bound(
            TempoMap.begin(),
            TempoMap.end(),
            tick,
            [](uint32_t value, TempoChange const& entry) noexcept { return value < entry.Tick; });

        auto const& entry = *(found == TempoMap.begin() ? found : std::prev(found));

        if (entry.MicrosecondsPerQuarterNote == 0)
        {
            return 60000000.0 / static_cast<double>(DefaultMicrosecondsPerQuarterNote);
        }

        return 60000000.0 / static_cast<double>(entry.MicrosecondsPerQuarterNote);
    }

    TimeSignatureChange const& MidiSequence::TimeSignatureAtTick(uint32_t tick) const noexcept
    {
        static TimeSignatureChange const fallback{};

        if (TimeSignatureMap.empty())
        {
            return fallback;
        }

        auto const found = std::upper_bound(
            TimeSignatureMap.begin(),
            TimeSignatureMap.end(),
            tick,
            [](uint32_t value, TimeSignatureChange const& entry) noexcept { return value < entry.Tick; });

        return *(found == TimeSignatureMap.begin() ? found : std::prev(found));
    }

    BarPosition MidiSequence::BarPositionAtTick(uint32_t tick) const noexcept
    {
        BarPosition position{};

        auto const& signature = TimeSignatureAtTick(tick);

        if (signature.TicksPerBar == 0 || tick < signature.Tick)
        {
            position.Bar = signature.BarNumberAtTick;
            return position;
        }

        auto const intoSection = tick - signature.Tick;

        position.Bar = signature.BarNumberAtTick + (intoSection / signature.TicksPerBar);

        auto const intoBar = intoSection % signature.TicksPerBar;
        auto const numerator = signature.Numerator == 0 ? 1u : static_cast<uint32_t>(signature.Numerator);
        auto const ticksPerBeat = signature.TicksPerBar / numerator;

        if (ticksPerBeat > 0)
        {
            position.Beat = (intoBar / ticksPerBeat) + 1;
            position.TicksIntoBeat = intoBar % ticksPerBeat;
        }

        return position;
    }

    size_t MidiSequence::FirstEventIndexAtTick(uint32_t tick) const noexcept
    {
        auto const found = std::lower_bound(
            Events.begin(),
            Events.end(),
            tick,
            [](SequenceEvent const& entry, uint32_t value) noexcept { return entry.Tick < value; });

        return static_cast<size_t>(std::distance(Events.begin(), found));
    }

    TextEvent const* MidiSequence::ChordSymbolAtTick(uint32_t tick) const noexcept
    {
        if (ChordSymbolIndexes.empty())
        {
            return nullptr;
        }

        auto const found = std::upper_bound(
            ChordSymbolIndexes.begin(),
            ChordSymbolIndexes.end(),
            tick,
            [this](uint32_t value, uint32_t index) noexcept { return value < TextEvents[index].Tick; });

        if (found == ChordSymbolIndexes.begin())
        {
            return nullptr;
        }

        return &TextEvents[*std::prev(found)];
    }

    _Use_decl_annotations_
    void MidiSequence::CollectSoundingNoteCounts(uint32_t tick, std::vector<uint8_t>& counts) const noexcept
    {
        try
        {
            counts.assign(Tracks.size(), 0);
        }
        catch (...)
        {
            return;
        }

        FillSoundingNoteCounts(tick, counts.data(), static_cast<uint32_t>(counts.size()));
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::FillSoundingNoteCounts(uint32_t tick, uint8_t* counts, uint32_t capacity) const noexcept
    {
        if (counts == nullptr || capacity == 0)
        {
            return 0;
        }

        auto const written = static_cast<uint32_t>(
            Tracks.size() < capacity ? Tracks.size() : static_cast<size_t>(capacity));

        std::fill(counts, counts + written, uint8_t{ 0 });

        if (Notes.empty() || Tracks.empty())
        {
            return written;
        }

        for (auto index = FirstNoteIndexForTickRange(tick); index < Notes.size(); ++index)
        {
            auto const& note = Notes[index];

            if (note.StartTick > tick)
            {
                break;
            }

            // A note is over at the instant it ends, not after it.
            if (note.EndTick <= tick || note.TrackIndex >= written)
            {
                continue;
            }

            if (counts[note.TrackIndex] < 255)
            {
                ++counts[note.TrackIndex];
            }
        }

        return written;
    }

    _Use_decl_annotations_
    size_t MidiSequence::FirstNoteIndexForTickRange(uint32_t startTick) const noexcept
    {
        auto const searchTick = startTick > LongestNoteTicks ? startTick - LongestNoteTicks : 0u;

        auto const found = std::lower_bound(
            Notes.begin(),
            Notes.end(),
            searchTick,
            [](Note const& note, uint32_t value) noexcept { return note.StartTick < value; });

        return static_cast<size_t>(std::distance(Notes.begin(), found));
    }

    _Use_decl_annotations_
    uint32_t MidiSequence::CountNotesInTickRange(uint32_t startTick, uint32_t endTick) const noexcept
    {
        uint32_t count{ 0 };

        ForEachNoteInTickRange(startTick, endTick, UINT32_MAX,
            [&count](Note const&) noexcept { ++count; });

        return count;
    }

    LyricLine const* MidiSequence::LyricLineAtTick(uint32_t tick) const noexcept
    {
        if (LyricLines.empty())
        {
            return nullptr;
        }

        auto const found = std::upper_bound(
            LyricLines.begin(),
            LyricLines.end(),
            tick,
            [](uint32_t value, LyricLine const& line) noexcept { return value < line.StartTick; });

        if (found == LyricLines.begin())
        {
            return nullptr;
        }

        return &*std::prev(found);
    }

    void MidiSequence::BuildLyricLines() noexcept
    {
        LyricLines.clear();

        try
        {
            // Files written as karaoke put the words in plain text events instead of lyric events,
            // and mark themselves with an "@K" tag. Without that tag a text event is far more
            // likely to be a copyright or a comment than something anyone wants to sing.
            bool hasLyricEvents = false;

            for (auto const& event : TextEvents)
            {
                if (event.Kind == TextKind::Lyric)
                {
                    hasLyricEvents = true;
                    break;
                }
            }

            if (!hasLyricEvents && !IsKaraoke)
            {
                return;
            }

            auto const wanted = hasLyricEvents ? TextKind::Lyric : TextKind::Text;

            // Files which mark their own line breaks are believed. The rest carry nothing but bare
            // syllables, and for those the only signal left is the rest between them: a singer's
            // line ends where the gap stops being part of the phrase. Word spacing cannot be
            // recovered the same way, because within a word and between words are the same length.
            bool marksItsOwnBreaks = false;

            for (auto const& event : TextEvents)
            {
                if (event.Kind != wanted || event.Text.empty())
                {
                    continue;
                }

                if (event.Text[0] == '/' || event.Text[0] == '\\' ||
                    event.Text.find_first_of("\r\n") != std::string::npos)
                {
                    marksItsOwnBreaks = true;
                    break;
                }
            }

            auto const gapForNewLine = static_cast<uint32_t>(NominalTicksPerQuarterNote(Division)) * 2;

            LyricLine current{};
            bool started = false;
            uint32_t previousTick = 0;

            auto const flush = [this, &current, &started]() noexcept
                {
                    if (started && !current.Text.empty())
                    {
                        LyricLines.push_back(current);
                    }

                    current = LyricLine{};
                    started = false;
                };

            for (auto const& event : TextEvents)
            {
                if (event.Kind != wanted || event.Text.empty())
                {
                    continue;
                }

                auto syllable = event.Text;

                // The karaoke tags carry the title and language, not words to sing.
                if (wanted == TextKind::Text && syllable[0] == '@')
                {
                    continue;
                }

                bool breakBefore = false;

                if (marksItsOwnBreaks)
                {
                    // A leading slash starts a line and a backslash starts a page.
                    if (syllable[0] == '/' || syllable[0] == '\\')
                    {
                        breakBefore = true;
                        syllable.erase(0, 1);
                    }
                    else if (syllable.find_first_of("\r\n") == 0)
                    {
                        breakBefore = true;
                        syllable.erase(0, syllable.find_first_not_of("\r\n"));
                    }
                }
                else if (started && event.Tick - previousTick >= gapForNewLine)
                {
                    breakBefore = true;
                }

                previousTick = event.Tick;

                // The break has to be taken before the empty check, because a file which marks its
                // line ends with a bare carriage return leaves nothing behind once it is removed.
                if (breakBefore)
                {
                    flush();
                }

                if (syllable.empty())
                {
                    continue;
                }

                if (!started)
                {
                    current.StartTick = event.Tick;
                    started = true;
                }

                for (auto const character : syllable)
                {
                    // Anything left is a break inside the syllable, which reads best as a space.
                    current.Text += (character == '\r' || character == '\n') ? ' ' : character;
                }

                if (current.Text.size() > MaximumLyricLineLength)
                {
                    flush();
                }
            }

            flush();

            for (size_t index = 0; index + 1 < LyricLines.size(); ++index)
            {
                LyricLines[index].EndTick = LyricLines[index + 1].StartTick;
            }

            if (!LyricLines.empty())
            {
                LyricLines.back().EndTick = LastTick;
            }
        }
        catch (...)
        {
            LyricLines.clear();
        }
    }

    _Use_decl_annotations_
    void MidiSequence::CollectGridLines(
        uint32_t startTick,
        uint32_t endTick,
        bool includeBeats,
        size_t maximum,
        std::vector<GridLine>& lines) const noexcept
    {
        try
        {
            lines.clear();
        }
        catch (...)
        {
            return;
        }

        if (endTick < startTick || maximum == 0)
        {
            return;
        }

        auto tick = startTick;

        while (tick <= endTick && lines.size() < maximum)
        {
            auto const& signature = TimeSignatureAtTick(tick);

            if (signature.TicksPerBar == 0 || signature.Numerator == 0)
            {
                return;
            }

            auto const beatTicks = signature.TicksPerBar / signature.Numerator;
            auto const step = (includeBeats && beatTicks > 0) ? beatTicks : signature.TicksPerBar;

            if (step == 0)
            {
                return;
            }

            // Align to this signature's own origin, or every line after a meter change lands in
            // the wrong place.
            auto const into = tick > signature.Tick ? tick - signature.Tick : 0u;
            auto const aligned = signature.Tick + ((into + step - 1) / step) * step;

            if (aligned > endTick)
            {
                return;
            }

            try
            {
                lines.push_back(GridLine{
                    aligned,
                    ((aligned - signature.Tick) % signature.TicksPerBar) == 0 });
            }
            catch (...)
            {
                return;
            }

            auto const next = aligned + step;

            if (next <= tick)
            {
                return;
            }

            tick = next;
        }
    }

    void MidiSequence::Finalize() noexcept
    {
        // The readers append in per track order, so one stable sort by tick gives the merged
        // timeline while keeping the within-a-tick order each track was written in.
        std::stable_sort(
            Events.begin(),
            Events.end(),
            [](SequenceEvent const& left, SequenceEvent const& right) noexcept { return left.Tick < right.Tick; });

        std::stable_sort(
            TextEvents.begin(),
            TextEvents.end(),
            [](TextEvent const& left, TextEvent const& right) noexcept { return left.Tick < right.Tick; });

        ChordSymbolIndexes.clear();

        for (uint32_t index = 0; index < TextEvents.size(); ++index)
        {
            if (TextEvents[index].Kind == TextKind::ChordSymbol)
            {
                ChordSymbolIndexes.push_back(index);
            }
        }

        auto const ticksPerQuarterNote = NominalTicksPerQuarterNote(Division);

        // ---- tempo map ----
        std::stable_sort(
            TempoMap.begin(),
            TempoMap.end(),
            [](TempoChange const& left, TempoChange const& right) noexcept { return left.Tick < right.Tick; });

        // Two tempo events at one tick is a real thing in files stitched together by an editor.
        // The last one written is the one that takes effect.
        if (TempoMap.size() > 1)
        {
            auto const removed = std::unique(
                TempoMap.rbegin(),
                TempoMap.rend(),
                [](TempoChange const& later, TempoChange const& earlier) noexcept { return later.Tick == earlier.Tick; });

            TempoMap.erase(TempoMap.begin(), removed.base());
        }

        if (TempoMap.empty() || TempoMap.front().Tick != 0)
        {
            TempoMap.insert(TempoMap.begin(), TempoChange{ 0, DefaultMicrosecondsPerQuarterNote, 0 });
        }

        {
            uint64_t microseconds = 0;
            uint32_t previousTick = 0;
            uint32_t previousTempo = DefaultMicrosecondsPerQuarterNote;

            for (auto& entry : TempoMap)
            {
                if (entry.MicrosecondsPerQuarterNote < MinimumMicrosecondsPerQuarterNote)
                {
                    entry.MicrosecondsPerQuarterNote = MinimumMicrosecondsPerQuarterNote;
                }

                microseconds += static_cast<uint64_t>(entry.Tick - previousTick) * previousTempo / ticksPerQuarterNote;

                entry.MicrosecondsAtTick = microseconds;

                previousTick = entry.Tick;
                previousTempo = entry.MicrosecondsPerQuarterNote;
            }
        }

        // ---- time signature map ----
        std::stable_sort(
            TimeSignatureMap.begin(),
            TimeSignatureMap.end(),
            [](TimeSignatureChange const& left, TimeSignatureChange const& right) noexcept { return left.Tick < right.Tick; });

        if (TimeSignatureMap.size() > 1)
        {
            auto const removed = std::unique(
                TimeSignatureMap.rbegin(),
                TimeSignatureMap.rend(),
                [](TimeSignatureChange const& later, TimeSignatureChange const& earlier) noexcept { return later.Tick == earlier.Tick; });

            TimeSignatureMap.erase(TimeSignatureMap.begin(), removed.base());
        }

        if (TimeSignatureMap.empty() || TimeSignatureMap.front().Tick != 0)
        {
            TimeSignatureMap.insert(TimeSignatureMap.begin(), TimeSignatureChange{});
        }

        {
            uint32_t bar = 1;
            uint32_t previousTick = 0;
            uint32_t previousTicksPerBar = 0;

            for (auto& entry : TimeSignatureMap)
            {
                entry.TicksPerBar = ComputeTicksPerBar(entry, ticksPerQuarterNote);

                if (previousTicksPerBar > 0 && entry.Tick > previousTick)
                {
                    bar += (entry.Tick - previousTick) / previousTicksPerBar;
                }

                entry.BarNumberAtTick = bar;

                previousTick = entry.Tick;
                previousTicksPerBar = entry.TicksPerBar;
            }
        }

        std::stable_sort(
            KeySignatureMap.begin(),
            KeySignatureMap.end(),
            [](KeySignatureChange const& left, KeySignatureChange const& right) noexcept { return left.Tick < right.Tick; });

        // ---- note pairing ----
        Notes.clear();

        {
            // Key is track, channel and note number. The value is a stack, because the same note
            // can legitimately be started twice before either end arrives.
            std::map<uint64_t, std::vector<size_t>> pending{};

            auto const keyOf = [](SequenceEvent const& event, uint8_t noteNumber) noexcept
                {
                    return (static_cast<uint64_t>(event.TrackIndex) << 32)
                        | (static_cast<uint64_t>(event.Channel) << 8)
                        | noteNumber;
                };

            for (auto const& event : Events)
            {
                if (event.Kind != EventKind::NoteOn && event.Kind != EventKind::NoteOff)
                {
                    continue;
                }

                auto const bytes = BytesOf(event);

                if (bytes.size() < 3)
                {
                    continue;
                }

                auto const noteNumber = static_cast<uint8_t>(bytes[1] & 0x7F);
                auto const velocity = static_cast<uint8_t>(bytes[2] & 0x7F);

                // A note on with zero velocity is a note off. Files rely on this to stay small.
                auto const isStart = event.Kind == EventKind::NoteOn && velocity > 0;

                auto const key = keyOf(event, noteNumber);

                if (isStart)
                {
                    Note note{};

                    note.StartTick = event.Tick;
                    note.EndTick = event.Tick;
                    note.TrackIndex = event.TrackIndex;
                    note.Channel = event.Channel;
                    note.NoteNumber = noteNumber;
                    note.Velocity = velocity;

                    pending[key].push_back(Notes.size());
                    Notes.push_back(note);
                }
                else
                {
                    auto const entry = pending.find(key);

                    if (entry != pending.end() && !entry->second.empty())
                    {
                        Notes[entry->second.back()].EndTick = event.Tick;
                        entry->second.pop_back();
                    }
                }
            }

            // A note whose end never arrived is held to the end of the sequence rather than
            // dropped, because that is what a player has to do with it anyway.
            uint32_t finalTick = Events.empty() ? 0 : Events.back().Tick;

            for (auto const& [key, indexes] : pending)
            {
                for (auto const index : indexes)
                {
                    Notes[index].EndTick = finalTick > Notes[index].StartTick ? finalTick : Notes[index].StartTick;
                }
            }
        }

        std::stable_sort(
            Notes.begin(),
            Notes.end(),
            [](Note const& left, Note const& right) noexcept { return left.StartTick < right.StartTick; });

        std::stable_sort(
            ProgramChanges.begin(),
            ProgramChanges.end(),
            [](ProgramChangeEvent const& left, ProgramChangeEvent const& right) noexcept { return left.Tick < right.Tick; });

        // ---- totals ----
        for (auto& track : Tracks)
        {
            track.ChannelMask = 0;
            track.NoteCount = 0;
            track.EventCount = 0;
        }

        UsedChannelMask = 0;
        LastTick = 0;

        for (auto const& event : Events)
        {
            if (event.Tick > LastTick)
            {
                LastTick = event.Tick;
            }

            if (event.TrackIndex < Tracks.size())
            {
                auto& track = Tracks[event.TrackIndex];

                ++track.EventCount;

                if (event.Tick > track.LastTick)
                {
                    track.LastTick = event.Tick;
                }

                if (event.Channel != ChannelNone && event.Channel < 16)
                {
                    track.ChannelMask |= static_cast<uint16_t>(1u << event.Channel);
                    UsedChannelMask |= static_cast<uint16_t>(1u << event.Channel);
                }
            }
        }

        for (auto const& note : Notes)
        {
            if (note.EndTick > LastTick)
            {
                LastTick = note.EndTick;
            }

            if (note.TrackIndex < Tracks.size())
            {
                ++Tracks[note.TrackIndex].NoteCount;
            }
        }

        if (Notes.empty())
        {
            LowestNote = 0;
            HighestNote = 127;
            LongestNoteTicks = 0;
        }
        else
        {
            LowestNote = 127;
            HighestNote = 0;
            LongestNoteTicks = 0;

            for (auto const& note : Notes)
            {
                if (note.NoteNumber < LowestNote) { LowestNote = note.NoteNumber; }
                if (note.NoteNumber > HighestNote) { HighestNote = note.NoteNumber; }

                auto const length = note.EndTick - note.StartTick;

                if (length > LongestNoteTicks) { LongestNoteTicks = length; }
            }
        }

        DurationMicroseconds = MicrosecondsAtTick(LastTick);

        if (Title.empty())
        {
            for (auto const& track : Tracks)
            {
                if (!track.Name.empty())
                {
                    Title = track.Name;
                    break;
                }
            }
        }

        // Last, because the closing line is given the end of the sequence to run to.
        BuildLyricLines();
    }

    void MidiSequence::Clear() noexcept
    {
        Format = SequenceFormat::MultiTrack;
        Division = TimeDivision{};

        Tracks.clear();
        Events.clear();
        EventBytes.clear();
        Notes.clear();
        TempoMap.clear();
        TimeSignatureMap.clear();
        KeySignatureMap.clear();
        TextEvents.clear();
        ProgramChanges.clear();
        ChordSymbolIndexes.clear();
        LyricLines.clear();

        LastTick = 0;
        DurationMicroseconds = 0;
        UsedChannelMask = 0;

        LowestNote = 0;
        HighestNote = 127;
        LongestNoteTicks = 0;

        Title.clear();
        Copyright.clear();

        IsKaraoke = false;
    }
}
