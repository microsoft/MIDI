// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceModelTests.h"
#include "SequenceTestData.h"

#include "SequenceRender.h"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;
using namespace midisequencer;

namespace
{
    constexpr int64_t Bar = TicksPerQuarterNote * 4;

    Sequence OneClip(Clip clip, std::vector<Placement> placements)
    {
        Sequence sequence{};

        Track track{};
        track.Id = L"t-1";
        track.Timeline = std::move(placements);

        sequence.Tracks = { track };
        sequence.Clips = { std::move(clip) };

        NormalizeSequence(sequence);
        return sequence;
    }

    std::vector<int64_t> TicksOf(std::vector<RenderedMessage> const& messages, RenderedKind kind)
    {
        std::vector<int64_t> ticks{};

        for (auto const& message : messages)
        {
            if (message.Kind == kind)
            {
                ticks.push_back(message.Tick);
            }
        }

        return ticks;
    }
}

void SequenceRenderTests::ALinkedClipPlaysAtEveryPlacement()
{
    auto const sequence = testdata::SampleSequence();
    auto const drums = FindTrack(sequence, L"t-drums");

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, *drums, 0, INT64_MAX, messages);

    auto const ons = TicksOf(messages, RenderedKind::NoteOn);

    // Six hits a bar: four bars from the first placement, which loops, and one from the second.
    VERIFY_ARE_EQUAL(size_t{ 30 }, ons.size());
    VERIFY_ARE_EQUAL(int64_t{ 0 }, ons.front());
    VERIFY_ARE_EQUAL(Bar * 8, ons[24]);
    VERIFY_ARE_EQUAL(size_t{ 30 }, TicksOf(messages, RenderedKind::NoteOff).size());

    VERIFY_ARE_EQUAL(Bar * 9, TrackEndTick(sequence, *drums));

    // Bass's second placement of its four-bar clip ends at bar 16.
    VERIFY_ARE_EQUAL(Bar * 16, SequenceEndTick(sequence));

    // A drum hit is a MIDI 2.0 note on channel 10 with the clip's 16-bit velocity.
    auto const& first = messages.front();
    VERIFY_ARE_EQUAL(uint8_t{ 2 }, first.WordCount);
    VERIFY_ARE_EQUAL(0x40992400u, first.Words[0]);
    VERIFY_ARE_EQUAL(0xFFFF0000u, first.Words[1]);
}

void SequenceRenderTests::ALoopRepeatsAndCutsNotesAtTheLoop()
{
    Clip clip{};
    clip.Id = L"c-1";
    clip.Length = Bar / 2;
    clip.Loop = true;
    clip.Notes = { testdata::MakeNote(1800, 500, 60) };

    auto const sequence = OneClip(clip, { Placement{ L"c-1", 0, Bar * 3 / 2 } });

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 0, INT64_MAX, messages);

    auto const ons = TicksOf(messages, RenderedKind::NoteOn);
    auto const offs = TicksOf(messages, RenderedKind::NoteOff);

    VERIFY_ARE_EQUAL(size_t{ 3 }, ons.size());
    VERIFY_ARE_EQUAL(int64_t{ 1800 }, ons[0]);
    VERIFY_ARE_EQUAL(int64_t{ 3720 }, ons[1]);
    VERIFY_ARE_EQUAL(int64_t{ 5640 }, ons[2]);

    VERIFY_ARE_EQUAL(int64_t{ 1920 }, offs[0]);
    VERIFY_ARE_EQUAL(int64_t{ 3840 }, offs[1]);
    VERIFY_ARE_EQUAL(int64_t{ 5760 }, offs[2]);

    // Without looping, the clip plays once and the rest of the placement is silent.
    clip.Loop = false;
    auto const once = OneClip(clip, { Placement{ L"c-1", 0, Bar * 3 / 2 } });

    messages.clear();
    RenderTrack(once, once.Tracks[0], 0, INT64_MAX, messages);
    VERIFY_ARE_EQUAL(size_t{ 1 }, TicksOf(messages, RenderedKind::NoteOn).size());
}

void SequenceRenderTests::NoteEndsComeBeforeNoteStartsAtOneTick()
{
    Clip clip{};
    clip.Id = L"c-1";
    clip.Length = Bar;
    clip.Notes = { testdata::MakeNote(0, 480, 60), testdata::MakeNote(480, 480, 60) };

    auto const sequence = OneClip(clip, { Placement{ L"c-1", 0, 0 } });

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 0, INT64_MAX, messages);

    VERIFY_ARE_EQUAL(size_t{ 4 }, messages.size());
    VERIFY_ARE_EQUAL(int64_t{ 480 }, messages[1].Tick);
    VERIFY_ARE_EQUAL(static_cast<int>(RenderedKind::NoteOff), static_cast<int>(messages[1].Kind));
    VERIFY_ARE_EQUAL(int64_t{ 480 }, messages[2].Tick);
    VERIFY_ARE_EQUAL(static_cast<int>(RenderedKind::NoteOn), static_cast<int>(messages[2].Kind));
}

void SequenceRenderTests::AWindowHoldsWhatStartsInIt()
{
    Clip clip{};
    clip.Id = L"c-1";
    clip.Length = Bar;
    clip.Loop = false;
    clip.Notes = {
        testdata::MakeNote(900, 500, 60),
        testdata::MakeNote(1000, 5000, 61),
        testdata::MakeNote(1999, 10, 62),
        testdata::MakeNote(2000, 10, 63),
    };
    clip.Events = { testdata::MakeEvent(1500, 0x20B00764), testdata::MakeEvent(2000, 0x20B00765) };

    auto const sequence = OneClip(clip, { Placement{ L"c-1", 0, 0 } });

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 1000, 2000, messages);

    auto const ons = TicksOf(messages, RenderedKind::NoteOn);
    auto const offs = TicksOf(messages, RenderedKind::NoteOff);

    VERIFY_ARE_EQUAL(size_t{ 2 }, ons.size());
    VERIFY_ARE_EQUAL(int64_t{ 1000 }, ons[0]);
    VERIFY_ARE_EQUAL(int64_t{ 1999 }, ons[1]);

    // A note comes with its end, even past the window. The clip isn't looping, so the long note
    // ends where the clip does.
    VERIFY_ARE_EQUAL(size_t{ 2 }, offs.size());
    VERIFY_ARE_EQUAL(int64_t{ 2009 }, offs[0]);
    VERIFY_ARE_EQUAL(Bar, offs[1]);

    VERIFY_ARE_EQUAL(size_t{ 1 }, TicksOf(messages, RenderedKind::Other).size());
}

void SequenceRenderTests::TheDestinationSetsGroupAndChannel()
{
    TrackDestination destination{};
    destination.Group = 5;
    destination.Channel = 9;

    std::array<uint32_t, 4> note{ 0x40903C00, 0xFFFF0000, 0, 0 };
    ApplyDestination(destination, note, 2);
    VERIFY_ARE_EQUAL(0x45993C00u, note[0]);
    VERIFY_ARE_EQUAL(0xFFFF0000u, note[1]);

    std::array<uint32_t, 4> controller{ 0x2AB30764, 0, 0, 0 };
    ApplyDestination(destination, controller, 1);
    VERIFY_ARE_EQUAL(0x25B90764u, controller[0]);

    // System exclusive has a group but no channel.
    std::array<uint32_t, 4> exclusive{ 0x3016437E, 0x7F060100, 0, 0 };
    ApplyDestination(destination, exclusive, 2);
    VERIFY_ARE_EQUAL(0x3516437Eu, exclusive[0]);

    // Utility and stream messages have neither.
    std::array<uint32_t, 4> utility{ 0x00400010, 0, 0, 0 };
    ApplyDestination(destination, utility, 1);
    VERIFY_ARE_EQUAL(0x00400010u, utility[0]);

    std::array<uint32_t, 4> stream{ 0xF0200000, 0, 0, 0 };
    ApplyDestination(destination, stream, 4);
    VERIFY_ARE_EQUAL(0xF0200000u, stream[0]);

    // As recorded: the group changes, each message keeps its channel.
    destination.Channel = -1;
    std::array<uint32_t, 4> recorded{ 0x40933C00, 0x80000000, 0, 0 };
    ApplyDestination(destination, recorded, 2);
    VERIFY_ARE_EQUAL(0x45933C00u, recorded[0]);
}

void SequenceRenderTests::AHostileLoopStopsAtTheCeiling()
{
    // A one-tick clip that loops for about two billion ticks.
    Clip clip{};
    clip.Id = L"c-1";
    clip.Length = 1;
    clip.Loop = true;
    clip.Notes = { testdata::MakeNote(0, 1, 60) };

    auto const sequence = OneClip(clip, { Placement{ L"c-1", 0, 0x7FFFFFF0 } });

    std::vector<RenderedMessage> messages{};
    RenderTrack(sequence, sequence.Tracks[0], 0, INT64_MAX, messages, 1000);

    VERIFY_ARE_EQUAL(size_t{ 1000 }, messages.size());
}
