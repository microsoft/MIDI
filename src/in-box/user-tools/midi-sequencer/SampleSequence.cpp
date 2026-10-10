// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "SampleSequence.h"
#include "SequenceEdits.h"

#include <algorithm>
#include <array>

namespace midisequencer
{
    namespace
    {
        constexpr int64_t Beat = TicksPerQuarterNote;
        constexpr int64_t Bar = TicksPerQuarterNote * 4;
        constexpr int64_t Eighth = TicksPerQuarterNote / 2;
        constexpr int64_t Sixteenth = TicksPerQuarterNote / 4;

        // The comps' mulberry32, so the sample's chance notes come out the same every time.
        class Random
        {
        public:
            explicit Random(uint32_t seed) noexcept : m_state(seed) {}

            uint32_t Next() noexcept
            {
                m_state += 0x6D2B79F5u;
                uint32_t t = m_state;
                t = (t ^ (t >> 15)) * (t | 1u);
                t ^= t + (t ^ (t >> 7)) * (t | 61u);
                return t ^ (t >> 14);
            }

            // 0 to count - 1
            uint32_t Below(uint32_t count) noexcept { return count == 0 ? 0 : Next() % count; }

        private:
            uint32_t m_state{ 0 };
        };

        uint16_t Velocity(double share) noexcept
        {
            return static_cast<uint16_t>(std::clamp(share, 0.0, 1.0) * 65535.0 + 0.5);
        }

        void AddNote(Clip& clip, int64_t tick, int64_t length, uint8_t number, double velocity, uint8_t chance = 100)
        {
            Note note{};
            note.Tick = tick;
            note.Length = length;
            note.Number = number;
            note.Velocity = Velocity(velocity);
            note.ReleaseVelocity = Velocity(0.5);
            note.Chance = chance;
            clip.Notes.push_back(note);
        }

        Clip NewClip(std::wstring const& name, int64_t length, uint32_t seed)
        {
            auto clip = MakeNotesClip(name, length);
            clip.Origin = ClipOrigin::Drawn;
            clip.Seed = seed;
            return clip;
        }

        // General MIDI drums, channel 10.
        constexpr uint8_t Kick = 36;
        constexpr uint8_t Snare = 38;
        constexpr uint8_t ClosedHat = 42;
        constexpr uint8_t OpenHat = 46;
        constexpr uint8_t Clap = 39;

        Clip BeatA(std::wstring const& name)
        {
            auto clip = NewClip(name, Bar, 11);

            for (int beat = 0; beat < 4; ++beat)
            {
                AddNote(clip, beat * Beat, Sixteenth, Kick, 0.88);
            }

            AddNote(clip, Beat, Sixteenth, Snare, 0.78);
            AddNote(clip, 3 * Beat, Sixteenth, Snare, 0.80);

            for (int step = 0; step < 8; ++step)
            {
                AddNote(clip, step * Eighth, Sixteenth, step == 7 ? OpenHat : ClosedHat, step % 2 == 0 ? 0.55 : 0.40);
            }

            return clip;
        }

        Clip BeatB(std::wstring const& name)
        {
            auto clip = NewClip(name, Bar, 12);

            AddNote(clip, 0, Sixteenth, Kick, 0.90);
            AddNote(clip, Beat + Eighth, Sixteenth, Kick, 0.70);
            AddNote(clip, 2 * Beat, Sixteenth, Kick, 0.86);
            AddNote(clip, Beat, Sixteenth, Snare, 0.80);
            AddNote(clip, 3 * Beat, Sixteenth, Clap, 0.82);

            for (int step = 0; step < 16; ++step)
            {
                AddNote(clip, step * Sixteenth, Sixteenth / 2, ClosedHat, step % 4 == 0 ? 0.55 : step % 2 == 0 ? 0.42 : 0.30);
            }

            return clip;
        }

        Clip BeatFill(std::wstring const& name)
        {
            auto clip = NewClip(name, Bar, 13);

            for (int beat = 0; beat < 3; ++beat)
            {
                AddNote(clip, beat * Beat, Sixteenth, Kick, 0.86);
                AddNote(clip, beat * Beat + Eighth, Sixteenth, ClosedHat, 0.40);
            }

            AddNote(clip, Beat, Sixteenth, Snare, 0.78);

            // A snare roll that builds into the next bar.
            for (int step = 0; step < 4; ++step)
            {
                AddNote(clip, 3 * Beat + step * Sixteenth, Sixteenth / 2, Snare, 0.50 + 0.12 * step);
            }

            return clip;
        }

        // E minor, C, D, B minor: roots for the bass, one bar each.
        constexpr std::array<uint8_t, 4> Roots{ 40, 36, 38, 35 };

        Clip BassLine(std::wstring const& name, bool variation, uint32_t seed)
        {
            auto clip = NewClip(name, 4 * Bar, seed);

            for (int bar = 0; bar < 4; ++bar)
            {
                auto const root = Roots[static_cast<size_t>(bar)];

                for (int step = 0; step < 8; ++step)
                {
                    // Root on the beat, the octave and the fifth around it.
                    uint8_t number = root;

                    if (step % 4 == 3)
                    {
                        number = static_cast<uint8_t>(root + 12);
                    }
                    else if (variation && step % 4 == 2)
                    {
                        number = static_cast<uint8_t>(root + 7);
                    }

                    if (!variation && step == 5)
                    {
                        continue;
                    }

                    AddNote(clip, bar * Bar + step * Eighth, Eighth - 60, number, step % 2 == 0 ? 0.70 : 0.55);
                }
            }

            return clip;
        }

        // Each chord as three notes, one bar each.
        constexpr std::array<std::array<uint8_t, 3>, 4> Chords{ { { 52, 55, 59 }, { 48, 52, 55 }, { 50, 54, 57 }, { 47, 50, 54 } } };

        Clip PadClip(std::wstring const& name, int pattern, uint32_t seed)
        {
            auto clip = NewClip(name, 4 * Bar, seed);

            for (int bar = 0; bar < 4; ++bar)
            {
                auto const& chord = Chords[static_cast<size_t>(bar)];

                for (auto const number : chord)
                {
                    switch (pattern)
                    {
                    case 0:     // held
                        AddNote(clip, bar * Bar, Bar - 40, static_cast<uint8_t>(number + 12), 0.50);
                        break;

                    case 1:     // two to a bar
                        AddNote(clip, bar * Bar, 2 * Beat - 40, static_cast<uint8_t>(number + 12), 0.62);
                        AddNote(clip, bar * Bar + 2 * Beat, 2 * Beat - 40, static_cast<uint8_t>(number + 12), 0.52);
                        break;

                    default:    // swelling
                        AddNote(clip, bar * Bar, Bar - 40, static_cast<uint8_t>(number + 12), 0.30 + 0.15 * bar);
                        break;
                    }
                }
            }

            return clip;
        }

        Clip Euclid(std::wstring const& name, int hits, uint32_t seed)
        {
            auto clip = NewClip(name, Bar, seed);
            constexpr std::array<uint8_t, 4> notes{ 64, 67, 71, 74 };
            size_t next{ 0 };

            for (int step = 0; step < 16; ++step)
            {
                // Bjorklund by the bucket method: a hit each time the running total wraps.
                if (((step * hits) % 16) < hits)
                {
                    AddNote(clip, step * Sixteenth, Sixteenth - 30, notes[next++ % notes.size()], 0.58);
                }
            }

            return clip;
        }

        Clip Wandering(std::wstring const& name, uint32_t seed)
        {
            auto clip = NewClip(name, 2 * Bar, seed);
            Random random{ seed };

            constexpr std::array<uint8_t, 7> scale{ 64, 66, 67, 69, 71, 72, 74 };
            int32_t position{ 3 };

            for (int step = 0; step < 16; ++step)
            {
                position = std::clamp(position + static_cast<int32_t>(random.Below(3)) - 1, 0, static_cast<int32_t>(scale.size()) - 1);
                auto const chance = static_cast<uint8_t>(step % 4 == 0 ? 100 : 65);
                AddNote(clip, step * Eighth, Eighth, scale[static_cast<size_t>(position)], 0.45 + 0.05 * (step % 3), chance);
            }

            return clip;
        }

        Clip LeadHook(std::wstring const& name)
        {
            auto clip = NewClip(name, 4 * Bar, 31);

            struct Step { int64_t Tick; int64_t Length; uint8_t Number; };
            constexpr std::array<Step, 12> steps{ {
                { 0, Beat, 76 }, { Beat, Eighth, 74 }, { Beat + Eighth, Eighth, 71 }, { 2 * Beat, 2 * Beat, 74 },
                { Bar, Beat, 72 }, { Bar + Beat, Beat, 71 }, { Bar + 2 * Beat, 2 * Beat, 67 },
                { 2 * Bar, Beat, 74 }, { 2 * Bar + Beat, Eighth, 76 }, { 2 * Bar + Beat + Eighth, Beat + Eighth, 79 },
                { 3 * Bar, 2 * Beat, 78 }, { 3 * Bar + 2 * Beat, 2 * Beat, 74 } } };

            for (auto const& step : steps)
            {
                AddNote(clip, step.Tick, step.Length - 30, step.Number, 0.72);
            }

            return clip;
        }

        Clip StabClip(std::wstring const& name)
        {
            auto clip = NewClip(name, Bar, 41);

            for (int beat = 0; beat < 4; ++beat)
            {
                for (auto const number : Chords[0])
                {
                    AddNote(clip, beat * Beat + Eighth, Sixteenth, static_cast<uint8_t>(number + 12), 0.66);
                }
            }

            return clip;
        }

        Clip Comping(std::wstring const& name)
        {
            auto clip = NewClip(name, 4 * Bar, 51);

            for (int bar = 0; bar < 4; ++bar)
            {
                for (auto const number : Chords[static_cast<size_t>(bar)])
                {
                    AddNote(clip, bar * Bar + Eighth, Beat, static_cast<uint8_t>(number + 12), 0.55);
                    AddNote(clip, bar * Bar + 2 * Beat + Eighth, Beat, static_cast<uint8_t>(number + 12), 0.48);
                }
            }

            return clip;
        }

        // A program change, as MIDI 1.0. The track's destination sets its group and channel.
        ClipEvent Program(uint8_t program)
        {
            ClipEvent event{};
            event.Words[0] = 0x20C00000u | (static_cast<uint32_t>(program & 0x7F) << 8);
            event.WordCount = 1;
            return event;
        }

        Placement Place(Clip const& clip, int64_t bar, int64_t bars)
        {
            auto const length = bars * Bar;
            return Placement{ clip.Id, (bar - 1) * Bar, length == clip.Length ? 0 : length };
        }
    }

    _Use_decl_annotations_
    Sequence MakeSampleSequence(SampleText const& text, EndpointRef const& synth)
    {
        Sequence sequence{};
        sequence.Name = text.SequenceName;

        // Steady at 124, then up to 128 into the drop.
        sequence.Tempo = { TempoPoint{ 0, 124.0, false }, TempoPoint{ 16 * Bar, 124.0, true }, TempoPoint{ 18 * Bar, 128.0, false } };
        sequence.Meter = { MeterChange{ 0, 4, 4 } };

        sequence.Tags = {
            Tag{ 0, text.SceneIntro, std::nullopt },
            Tag{ 4 * Bar, text.SceneGroove, std::nullopt },
            Tag{ 12 * Bar, text.SceneBreak, std::nullopt },
            Tag{ 16 * Bar, text.SceneDrop, 0xF2C14E },
            Tag{ 22 * Bar, text.SceneOutro, std::nullopt } };

        for (auto const* name : { &text.SceneIntro, &text.SceneGroove, &text.SceneBreak, &text.SceneDrop, &text.SceneOutro })
        {
            sequence.Scenes.push_back(Scene{ NewId(L"s"), *name, nullptr });
        }

        auto const scenes = sequence.Scenes.size();

        // A program below 0 sends none.
        auto const makeTrack = [&](std::wstring const& name, uint32_t color, int8_t channel, int program)
        {
            auto track = MakeTrack(sequence, name, false);
            track.Color = color;
            track.Destination.Endpoint = synth;
            track.Destination.Channel = channel;
            track.Slots.resize(scenes);

            if (program >= 0)
            {
                track.Startup.push_back(Program(static_cast<uint8_t>(program)));
            }

            return track;
        };

        auto const makeFolder = [&](std::wstring const& name, uint32_t color, bool open)
        {
            auto folder = MakeTrack(sequence, name, true);
            folder.Color = color;
            folder.Open = open;
            return folder;
        };

        auto const add = [&](Clip clip)
        {
            sequence.Clips.push_back(std::move(clip));
            return sequence.Clips.back();
        };

        // Drums, pinned to the top.
        auto const beatA = add(BeatA(text.BeatA));
        auto const beatB = add(BeatB(text.BeatB));
        auto const fill = add(BeatFill(text.BeatFill));

        auto drums = makeTrack(text.Drums, 0xFF8A65, 9, -1);
        drums.Pinned = true;
        drums.Timeline = { Place(beatA, 1, 4), Place(beatA, 5, 4), Place(beatA, 9, 4), Place(beatB, 17, 4), Place(beatA, 21, 4) };
        drums.Slots = { beatA.Id, beatA.Id, L"", beatB.Id, fill.Id };

        // Synths
        auto const bassA = add(BassLine(text.BassLineA, false, 21));
        auto const bassB = add(BassLine(text.BassLineB, true, 22));
        auto const padIntro = add(PadClip(text.PadIntro, 0, 23));
        auto const padChords = add(PadClip(text.PadChords, 1, 24));
        auto const padSwell = add(PadClip(text.PadSwell, 2, 25));
        auto const euclidFive = add(Euclid(text.EuclidFive, 5, 26));
        auto const euclidSeven = add(Euclid(text.EuclidSeven, 7, 27));
        auto const wander = add(Wandering(text.Wander, 0x4F2A));
        auto const drift = add(Wandering(text.Drift, 0x51C3));

        auto synths = makeFolder(text.Synths, 0x9C8CFF, true);

        auto bass = makeTrack(text.Bass, 0x5BC0EB, 1, 38);
        bass.Timeline = { Place(bassA, 5, 4), Place(bassA, 9, 4), Place(bassB, 13, 4), Place(bassA, 17, 4), Place(bassA, 21, 4) };
        bass.Slots = { L"", bassA.Id, bassB.Id, bassA.Id, L"" };
        bass.Tags = { Tag{ 16 * Bar, text.TagFilterOpens, std::nullopt } };

        auto pad = makeTrack(text.Pad, 0xB4A7FF, 2, 89);
        pad.Timeline = { Place(padIntro, 1, 4), Place(padChords, 5, 8), Place(padSwell, 13, 4), Place(padChords, 17, 8) };
        pad.Slots = { padIntro.Id, padChords.Id, L"", padSwell.Id, L"" };

        auto arp = makeTrack(text.Arp, 0x6CCB5F, 3, 81);
        arp.Timeline = { Place(euclidFive, 9, 16) };
        arp.Slots = { L"", euclidFive.Id, L"", euclidSeven.Id, L"" };

        auto texture = makeTrack(text.Texture, 0x4DD0C4, 4, 95);
        texture.Timeline = { Place(wander, 13, 12) };
        texture.Slots = { L"", L"", wander.Id, drift.Id, L"" };

        synths.Children = { std::move(bass), std::move(pad), std::move(arp), std::move(texture) };

        // Keys, closed.
        auto const comping = add(Comping(text.Comping));
        auto const organPads = add(PadClip(text.OrganPads, 0, 61));

        auto keys = makeFolder(text.Keys, 0xF7C948, false);

        auto rhodes = makeTrack(text.Rhodes, 0xF7C948, 5, 4);
        rhodes.Timeline = { Place(comping, 5, 8) };

        auto organ = makeTrack(text.Organ, 0xF7C948, 6, 16);
        organ.Timeline = { Place(organPads, 17, 4) };

        keys.Children = { std::move(rhodes), std::move(organ) };

        // Lead and Chords. Chords starts out muted.
        auto const hook = add(LeadHook(text.LeadHook));
        auto const stabs = add(StabClip(text.Stabs));

        auto lead = makeTrack(text.Lead, 0xFF6F91, 7, 80);
        lead.Timeline = { Place(hook, 9, 4), Place(hook, 17, 4) };
        lead.Slots = { hook.Id, L"", L"", L"", L"" };
        lead.Tags = { Tag{ 10 * Bar, text.TagRetake, std::nullopt } };

        auto chords = makeTrack(text.Chords, 0xFFB547, 8, 62);
        chords.Muted = true;
        chords.Timeline = { Place(stabs, 5, 8), Place(stabs, 17, 8) };
        chords.Slots = { L"", stabs.Id, L"", stabs.Id, L"" };

        sequence.Tracks = { std::move(drums), std::move(synths), std::move(keys), std::move(lead), std::move(chords) };

        NormalizeSequence(sequence);
        return sequence;
    }
}
