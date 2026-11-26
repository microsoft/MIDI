// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "HostileFileTests.h"

#include "Lyrics.h"
#include "SequenceRender.h"

#include <chrono>
#include <string>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    // A one-tick looping clip placed for as long as a file may say.
    Sequence OneTickLoop(bool withNote)
    {
        Sequence sequence{};

        Clip clip{};
        clip.Id = L"c-tiny";
        clip.Length = 1;
        clip.Loop = true;

        if (withNote)
        {
            Note note{};
            note.Length = 1;
            note.Number = 60;
            note.Velocity = 0xC000;
            clip.Notes.push_back(note);
        }

        sequence.Clips.push_back(clip);

        Track track{};
        track.Id = L"t-tiny";
        track.Timeline.push_back(Placement{ clip.Id, 0, SequenceLimits{}.MaximumTick });
        sequence.Tracks.push_back(track);

        return sequence;
    }

    double SecondsSince(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
}

void HostileFileTests::AnEmptyOneTickLoopRendersQuickly()
{
    auto const sequence = OneTickLoop(false);

    std::vector<RenderedMessage> messages{};
    auto const start = std::chrono::steady_clock::now();

    // An export renders everything, and starting late in the song renders what came before.
    RenderTrack(sequence, sequence.Tracks[0], 0, INT64_MAX, messages);

    Log::Comment(String().Format(L"Rendered in %.3f s", SecondsSince(start)));
    VERIFY_IS_TRUE(messages.empty());
    VERIFY_IS_LESS_THAN(SecondsSince(start), 5.0);
}

void HostileFileTests::AOneTickLoopWithANoteStopsAtTheLimit()
{
    auto const sequence = OneTickLoop(true);

    std::vector<RenderedMessage> messages{};
    auto const start = std::chrono::steady_clock::now();

    RenderTrack(sequence, sequence.Tracks[0], 0, INT64_MAX, messages, 10000);

    VERIFY_IS_LESS_THAN_OR_EQUAL(messages.size(), size_t{ 10002 });
    VERIFY_IS_LESS_THAN(SecondsSince(start), 5.0);
}

void HostileFileTests::LyricsFromAHugeFileStopAtTheLimit()
{
    SequenceLimits limits{};
    limits.MaximumLyrics = 1000;

    std::vector<LyricFileText> events(5000, LyricFileText{ 0, std::string(100000, 'a') });

    auto const start = std::chrono::steady_clock::now();
    auto const syllables = LyricsFromFileText(events, false, limits);

    VERIFY_ARE_EQUAL(size_t{ 1000 }, syllables.size());
    VERIFY_ARE_EQUAL(limits.MaximumTextLength, syllables.front().Text.size());
    VERIFY_IS_LESS_THAN(SecondsSince(start), 5.0);
}
