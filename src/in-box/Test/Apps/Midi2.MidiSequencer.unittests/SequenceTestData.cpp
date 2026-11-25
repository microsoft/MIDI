// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SequenceTestData.h"

using namespace midisequencer;

namespace testdata
{
    Note MakeNote(int64_t tick, int64_t length, uint8_t number, uint16_t velocity, uint8_t channel)
    {
        Note note{};
        note.Tick = tick;
        note.Length = length;
        note.Number = number;
        note.Velocity = velocity;
        note.Channel = channel;
        return note;
    }

    ClipEvent MakeEvent(int64_t tick, uint32_t word0, uint32_t word1)
    {
        ClipEvent event{};
        event.Tick = tick;
        event.Words[0] = word0;
        event.Words[1] = word1;
        event.WordCount = UmpWordCount(word0);
        return event;
    }

    Sequence SampleSequence()
    {
        constexpr int64_t bar = TicksPerQuarterNote * 4;

        Sequence sequence{};
        sequence.Name = L"Night Drive Sketch";

        midiapp::ContentProvenance provenance{};
        provenance.Id = L"0b7c6f3e-4e1a-4c8e-9a51-2f3d7c9e1a24";
        provenance.Author = L"Pat Example";
        provenance.License = L"CC-BY-4.0";
        provenance.DigitalSourceType = midiapp::DigitalSourceTypes::Composite;
        sequence.Provenance = provenance;

        sequence.Tempo = { TempoPoint{ 0, 124.0, false }, TempoPoint{ bar * 16, 124.0, true }, TempoPoint{ bar * 18, 128.0, false } };
        sequence.Meter = { MeterChange{ 0, 4, 4 }, MeterChange{ bar * 20, 7, 8 } };
        sequence.Tags = { Tag{ 0, L"Intro", std::nullopt }, Tag{ bar * 16, L"Drop", 0xF2C14E } };
        sequence.Scenes = { Scene{ L"s-intro", L"Intro", nullptr }, Scene{ L"s-groove", L"Groove", nullptr } };

        Clip beat{};
        beat.Id = L"c-beat-a";
        beat.Kind = ClipKind::Notes;
        beat.Name = L"Beat A";
        beat.Length = bar;
        beat.Loop = true;
        beat.Origin = ClipOrigin::Drawn;

        for (int64_t step = 0; step < 16; step += 4)
        {
            beat.Notes.push_back(MakeNote(step * 240, 120, 36, 0xFFFF, 9));
        }

        beat.Notes.push_back(MakeNote(960, 120, 38, 0xA000, 9));
        beat.Notes.push_back(MakeNote(2880, 120, 38, 0xA000, 9));

        Clip bass{};
        bass.Id = L"c-bass-a";
        bass.Name = L"Bass line A";
        bass.Length = bar * 4;
        bass.Loop = true;
        bass.Seed = 0x4F2A;
        bass.Origin = ClipOrigin::Recorded;
        bass.OriginDetail = L"Keystep 37";
        bass.Notes = {
            MakeNote(0, 403, 40, 0xFFFF, 1),
            MakeNote(480, 403, 40, 0x8CCC, 1),
            MakeNote(960, 2000, 52, 0x6000, 1),
        };
        bass.Notes[2].ReleaseVelocity = 0x4000;
        bass.Notes[1].Chance = 60;

        // A 32-bit controller and a per-note pitch bend, which MIDI 1.0 can't carry.
        bass.Events = {
            MakeEvent(0, 0x40B14A00, 0x80000000),
            MakeEvent(960, 0x40613400, 0x90000000),
        };

        Track drums{};
        drums.Id = L"t-drums";
        drums.Name = L"Drums";
        drums.Color = 0xFF8A65;
        drums.Pinned = true;
        drums.Destination.Endpoint = EndpointRef{ L"TR-8S", L"" };
        drums.Destination.Channel = 9;
        drums.Timeline = { Placement{ L"c-beat-a", 0, bar * 4 }, Placement{ L"c-beat-a", bar * 8, 0 } };
        drums.Slots = { L"c-beat-a", L"" };

        Track bassTrack{};
        bassTrack.Id = L"t-bass";
        bassTrack.Name = L"Bass";
        bassTrack.Color = 0x5BC0EB;
        bassTrack.Source.Endpoint = EndpointRef{ L"Keystep 37", L"" };
        bassTrack.Source.Channels = 0x0003;
        bassTrack.Destination.Endpoint = EndpointRef{ L"Moog One", L"\\\\?\\swd#midisrv#midiu_ksa_example#{e7cce071-3c03-423f-88d3-f1045d02552b}" };
        bassTrack.Destination.Group = 2;
        bassTrack.Destination.Channel = 1;
        bassTrack.Destination.Protocol = ProtocolChoice::Midi1;
        bassTrack.Startup = { MakeEvent(0, 0x20C15100), MakeEvent(0, 0x20B10764) };
        bassTrack.Timeline = { Placement{ L"c-bass-a", bar * 4, 0 }, Placement{ L"c-bass-a", bar * 12, 0 } };
        bassTrack.Slots = { L"", L"c-bass-a" };
        bassTrack.Tags = { Tag{ bar * 16, L"Filter opens here", 0x9FDFFF } };

        Track pad{};
        pad.Id = L"t-pad";
        pad.Name = L"Pad";
        pad.Color = 0xB4A7FF;
        pad.Muted = true;
        pad.Destination.Channel = -1;

        Track synths{};
        synths.Id = L"f-synths";
        synths.IsFolder = true;
        synths.Name = L"Synths";
        synths.Color = 0x9C8CFF;
        synths.Children = { bassTrack, pad };

        sequence.Tracks = { drums, synths };
        sequence.Clips = { beat, bass };

        NormalizeSequence(sequence);
        return sequence;
    }

    Sequence LargeSequence(size_t noteCount, size_t eventCount)
    {
        Sequence sequence{};
        sequence.Name = L"Large";

        Clip clip{};
        clip.Id = L"c-large";
        clip.Name = L"Large recording";
        clip.Loop = false;

        uint32_t seed{ 12345 };
        auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed; };

        clip.Notes.reserve(noteCount);

        for (size_t i = 0; i < noteCount; ++i)
        {
            auto const tick = static_cast<int64_t>(i) * 120 + (next() % 30);
            clip.Notes.push_back(MakeNote(tick, 60 + (next() % 900), static_cast<uint8_t>(36 + next() % 48), static_cast<uint16_t>(next() & 0xFFFF), 0));
        }

        clip.Events.reserve(eventCount);

        for (size_t i = 0; i < eventCount; ++i)
        {
            clip.Events.push_back(MakeEvent(static_cast<int64_t>(i) * 40, 0x40B00100, next()));
        }

        clip.Length = static_cast<int64_t>(noteCount) * 120 + TicksPerQuarterNote * 4;

        Track track{};
        track.Id = L"t-large";
        track.Name = L"Keys";
        track.Timeline = { Placement{ clip.Id, 0, 0 } };

        sequence.Tracks = { track };
        sequence.Clips = { clip };

        NormalizeSequence(sequence);
        return sequence;
    }
}
