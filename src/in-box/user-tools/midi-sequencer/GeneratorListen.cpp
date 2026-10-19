// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GeneratorListen.h"

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"
#include "SequenceRender.h"

#include <algorithm>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace midisequencer
{
    namespace
    {
        // What was heard before the passes playing now, for every listener at once. A loop keeps
        // two entries a pass, so this holds a few hundred passes of several loops.
        constexpr size_t RecentSize = 1024;

        // Every 32nd pass is kept longer, so a jump back works out at most 32 passes again.
        constexpr uint64_t KeptEvery = 32;
        constexpr size_t MaximumKept = 8192;

        // How deep hearing may go: a listener hearing a listener hearing another. Loops are worked
        // out oldest pass first, so they stay shallow; this stops anything else running away.
        constexpr int32_t MaximumDepth = 16;

        // The most one pass's hearing renders.
        constexpr size_t MaximumHeardMessages = 20000;

        // The most a source is learned from at once: 64 bars of 4/4.
        constexpr int64_t MaximumLearnedTicks = TicksPerQuarterNote * 4 * 64;

        thread_local int32_t t_depth{ 0 };

        struct HeardEntry
        {
            uint64_t Key{ 0 };
            Hearing Heard{};
        };

        std::mutex g_lock{};
        std::vector<HeardEntry> g_recent{};
        size_t g_next{ 0 };
        std::unordered_map<uint64_t, Hearing> g_kept{};

        // FNV-1a, a field at a time, like the generators' own fingerprint.
        struct Fnv
        {
            uint64_t Value{ 0xCBF29CE484222325ull };

            void Add(_In_ uint64_t value) noexcept
            {
                for (int32_t i = 0; i < 8; ++i)
                {
                    Value ^= (value >> (i * 8)) & 0xFF;
                    Value *= 0x100000001B3ull;
                }
            }

            void Add(_In_ std::wstring_view text) noexcept
            {
                Add(text.size());

                for (auto const ch : text)
                {
                    Add(static_cast<uint64_t>(ch));
                }
            }
        };

        // Everything a listener can hear, followed through the listening graph: each source's
        // notes and settings, a track's placements and their clips, and what those listen to in
        // turn. Loops says whether the listener can hear itself.
        struct Walk
        {
            Sequence const& Document;
            std::wstring_view Listener;
            Fnv Print{};
            std::vector<std::wstring> Seen{};
            bool Loops{ false };

            // A track can be heard: playback hears one as it plays now.
            bool Tracks{ false };

            bool FirstVisit(_In_ std::wstring const& mark)
            {
                if (std::find(Seen.begin(), Seen.end(), mark) != Seen.end())
                {
                    Print.Add(1);
                    Print.Add(mark);
                    return false;
                }

                Seen.push_back(mark);
                return true;
            }

            void VisitSource(_In_ ListenKind kind, _In_ std::wstring_view id)
            {
                Print.Add(static_cast<uint64_t>(kind));

                if (kind == ListenKind::Clip)
                {
                    VisitClip(id);
                }
                else if (kind == ListenKind::Track)
                {
                    VisitTrack(id);
                }
            }

            void VisitTrack(_In_ std::wstring_view id)
            {
                Tracks = true;

                if (!FirstVisit(L"t:" + std::wstring{ id }))
                {
                    return;
                }

                auto const track = FindTrack(Document, id);

                if (track == nullptr)
                {
                    Print.Add(0);
                    return;
                }

                Print.Add(id);
                Print.Add(track->Timeline.size());

                for (auto const& placement : track->Timeline)
                {
                    Print.Add(placement.ClipId);
                    Print.Add(static_cast<uint64_t>(placement.Tick));
                    Print.Add(static_cast<uint64_t>(placement.Length));
                    VisitClip(placement.ClipId);
                }
            }

            void VisitClip(_In_ std::wstring_view id)
            {
                if (id == Listener)
                {
                    Loops = true;
                }

                if (!FirstVisit(L"c:" + std::wstring{ id }))
                {
                    return;
                }

                auto const clip = FindClip(Document, id);

                if (clip == nullptr)
                {
                    Print.Add(0);
                    return;
                }

                Print.Add(id);
                Print.Add(static_cast<uint64_t>(clip->Kind));
                Print.Add(static_cast<uint64_t>(clip->Length));
                Print.Add(clip->Loop ? 1 : 0);
                Print.Add(clip->Seed);
                Print.Add(clip->Notes.size());

                for (auto const& note : clip->Notes)
                {
                    Print.Add(static_cast<uint64_t>(note.Tick));
                    Print.Add(static_cast<uint64_t>(note.Length));
                    Print.Add((static_cast<uint64_t>(note.Channel) << 40) | (static_cast<uint64_t>(note.Number) << 32) | (static_cast<uint64_t>(note.Velocity) << 8) | note.Chance);
                }

                if (clip->Pattern.has_value())
                {
                    auto const& pattern = *clip->Pattern;
                    Print.Add((static_cast<uint64_t>(pattern.Mode) << 48) | (static_cast<uint64_t>(pattern.StepCount) << 32) | (static_cast<uint64_t>(pattern.Swing) << 16) |
                        (static_cast<uint64_t>(pattern.Channel) << 8) | (pattern.LanesKeepLength ? 1 : 0));
                    Print.Add(static_cast<uint64_t>(pattern.StepTicks));

                    for (auto const& lane : pattern.Lanes)
                    {
                        Print.Add((static_cast<uint64_t>(lane.Note) << 32) | (static_cast<uint64_t>(lane.StepCount) << 8) | (lane.Muted ? 2 : 0) | (lane.Soloed ? 1 : 0));
                        Print.Add(lane.Steps.size());

                        for (auto const& step : lane.Steps)
                        {
                            Print.Add((static_cast<uint64_t>(step.Index) << 48) | (static_cast<uint64_t>(step.Velocity) << 32) | (static_cast<uint64_t>(step.Chance) << 24) |
                                (static_cast<uint64_t>(step.Ratchet) << 16) | (static_cast<uint64_t>(step.Shape) << 8) | static_cast<uint64_t>(step.Condition));
                            Print.Add((static_cast<uint64_t>(step.Every) << 56) | (static_cast<uint64_t>(step.Of) << 48) | (static_cast<uint64_t>(static_cast<uint16_t>(step.Nudge)) << 32) |
                                (static_cast<uint64_t>(step.Length) << 16) | (static_cast<uint64_t>(step.Note) << 8) | (step.Accent ? 2 : 0) | (step.Slide ? 1 : 0));
                        }
                    }
                }

                if (clip->Generator.has_value())
                {
                    auto const& generator = *clip->Generator;
                    Print.Add(GeneratorFingerprint(generator, clip->Seed, GeneratorStepCount(*clip)));
                    Print.Add(static_cast<uint64_t>(generator.StepTicks));
                    Print.Add((static_cast<uint64_t>(generator.NoteLength) << 8) | generator.Channel);

                    // What the generators' own fingerprint leaves out: the rings and the arpeggiator.
                    for (auto const& ring : generator.Rings)
                    {
                        Print.Add((static_cast<uint64_t>(ring.Note) << 56) | (static_cast<uint64_t>(ring.Pulses) << 48) | (static_cast<uint64_t>(ring.Steps) << 40) |
                            (static_cast<uint64_t>(ring.Rotate) << 32) | (static_cast<uint64_t>(ring.Velocity) << 8) | ring.Chance);
                    }

                    Print.Add((static_cast<uint64_t>(generator.ArpSource) << 24) | (static_cast<uint64_t>(generator.ArpOrder) << 16) | (static_cast<uint64_t>(generator.ArpOctaves) << 8) | (generator.ArpLatch ? 1 : 0));

                    if (generator.Type == GeneratorType::Arpeggiator && generator.ArpSource == ArpeggiatorSource::Track)
                    {
                        VisitTrack(generator.ArpTrackId);
                    }

                    // And whatever it listens to, in turn.
                    Print.Add(generator.KeepListening ? 1 : 0);

                    if (ListensEachPass(generator))
                    {
                        VisitSource(generator.ListenTo, generator.ListenId);
                    }
                }
            }
        };

        // What a listener heard before a pass is kept by this: the listener, the pass, where it
        // plays when its source is a track, and everything it can hear.
        uint64_t HeardKey(_In_ Clip const& clip, _In_ uint64_t pass, _In_ int64_t passStart, _In_ uint64_t heard) noexcept
        {
            auto const& generator = *clip.Generator;

            Fnv print{};
            print.Add(GeneratorFingerprint(generator, clip.Seed, GeneratorStepCount(clip)));
            print.Add(clip.Id);
            print.Add(static_cast<uint64_t>(clip.Length));
            print.Add(static_cast<uint64_t>(generator.StepTicks));
            print.Add(pass);

            // A clip is heard side by side wherever the listener plays; a track where it plays.
            print.Add(generator.ListenTo == ListenKind::Track ? static_cast<uint64_t>(passStart) : 0);
            print.Add(heard);
            return print.Value;
        }

        std::optional<Hearing> FindHeard(_In_ uint64_t key)
        {
            std::lock_guard guard{ g_lock };

            for (auto const& entry : g_recent)
            {
                if (entry.Key == key)
                {
                    return entry.Heard;
                }
            }

            if (auto const kept = g_kept.find(key); kept != g_kept.end())
            {
                return kept->second;
            }

            return std::nullopt;
        }

        void KeepHeard(_In_ uint64_t key, _In_ bool longer, _In_ Hearing const& heard)
        {
            std::lock_guard guard{ g_lock };

            if (g_recent.size() < RecentSize)
            {
                g_recent.push_back(HeardEntry{ key, heard });
            }
            else
            {
                g_recent[g_next] = HeardEntry{ key, heard };
                g_next = (g_next + 1) % RecentSize;
            }

            if (longer)
            {
                if (g_kept.size() >= MaximumKept)
                {
                    g_kept.clear();
                }

                g_kept[key] = heard;
            }
        }

        // Everything a listener can hear, worked out once for all the passes it asks about.
        struct Graph
        {
            uint64_t Print{ 0 };        // the listening graph
            uint64_t Heard{ 0 };        // the same, and for playback, what it has launched now
            bool Live{ false };
            bool Loops{ false };
        };

        Graph GraphFor(_In_ ListenContext const& context, _In_ Clip const& clip)
        {
            auto const& generator = *clip.Generator;

            Walk walk{ *context.Document, clip.Id };
            walk.VisitSource(generator.ListenTo, generator.ListenId);

            // Playback hears a track as it plays now, from its timeline or a clip launched on it, so
            // what anything that can hear a track heard there is kept only while those stay the same.
            Graph graph{};
            graph.Print = walk.Print.Value;
            graph.Live = context.HearTrack != nullptr && *context.HearTrack && walk.Tracks;
            graph.Heard = graph.Live ? graph.Print ^ (context.Epoch * 0x9E3779B97F4A7C15ull) : graph.Print;
            graph.Loops = walk.Loops;
            return graph;
        }

        // Notes heard from rom on, on a listener's steps: each starts on its nearest step, and
        // one that rounds past the last step wraps to the first, as a board's edges do.
        std::vector<StepNote> StepNotesFrom(_In_ std::vector<Note> const& notes, _In_ int64_t from, _In_ int64_t stepTicks, _In_ int64_t steps)
        {
            stepTicks = std::max<int64_t>(1, stepTicks);
            steps = std::max<int64_t>(1, steps);

            std::vector<StepNote> heard{};

            for (auto const& note : notes)
            {
                if (heard.size() >= MaximumLearnedNotes)
                {
                    break;
                }

                StepNote step{};
                step.Step = ((std::max<int64_t>(0, note.Tick - from) + stepTicks / 2) / stepTicks) % steps;
                step.Steps = std::clamp<int64_t>((note.Length + stepTicks / 2) / stepTicks, 1, steps);
                step.Number = static_cast<uint8_t>(note.Number & 0x7F);
                step.Velocity = std::max<uint16_t>(1, note.Velocity);
                heard.push_back(step);
            }

            return heard;
        }

        // The notes a track plays, from the one render path: each note on with its note off.
        std::vector<Note> RenderedNotes(_In_ ListenContext const& context, _In_ Track const& track, _In_ int64_t fromTick, _In_ int64_t toTick)
        {
            std::vector<RenderedMessage> messages{};
            RenderOptions options{};
            options.Lanes = false;
            options.HearTrack = context.HearTrack;
            options.HearEpoch = context.Epoch;
            options.MaximumMessages = MaximumHeardMessages;
            RenderTrack(*context.Document, track, fromTick, toTick, messages, options);

            std::vector<Note> notes{};

            for (size_t i = 0; i < messages.size(); ++i)
            {
                auto const& message = messages[i];

                if (message.Kind != RenderedKind::NoteOn || message.Tick < fromTick || message.Tick >= toTick)
                {
                    continue;
                }

                Note note{};
                note.Tick = message.Tick;
                note.Channel = static_cast<uint8_t>((message.Words[0] >> 16) & 0x0F);
                note.Number = static_cast<uint8_t>((message.Words[0] >> 8) & 0x7F);
                note.Velocity = std::max<uint16_t>(1, static_cast<uint16_t>(message.Words[1] >> 16));
                note.Length = 1;

                // Its end: the first note off of the same note on the same channel after it.
                for (auto j = i + 1; j < messages.size(); ++j)
                {
                    auto const& off = messages[j];

                    if (off.Kind == RenderedKind::NoteOff && ((off.Words[0] >> 8) & 0x0FFF) == ((message.Words[0] >> 8) & 0x0FFF))
                    {
                        note.Length = std::max<int64_t>(1, off.Tick - note.Tick);
                        break;
                    }
                }

                notes.push_back(note);
            }

            return notes;
        }
    }

    _Use_decl_annotations_
    bool CanListen(GeneratorType type) noexcept
    {
        switch (type)
        {
        case GeneratorType::Learn:
        case GeneratorType::Life:
        case GeneratorType::LifeDrums:
        case GeneratorType::Evolve:
        case GeneratorType::ChangeRinging:
            return true;

        default:
            return false;
        }
    }

    _Use_decl_annotations_
    bool ListensEachPass(GeneratorSettings const& generator) noexcept
    {
        // Learn listens only while it learns from a clip or a track, rather than a tune or words.
        return CanListen(generator.Type) && generator.KeepListening && generator.ListenTo != ListenKind::None && !generator.ListenId.empty() &&
            (generator.Type != GeneratorType::Learn || generator.LearnFrom == LearnSource::Clip);
    }

    _Use_decl_annotations_
    std::vector<Note> NotesHeard(ListenContext const& context, ListenKind kind, std::wstring_view id, int64_t fromTick, int64_t toTick)
    {
        if (context.Document == nullptr || id.empty() || toTick <= fromTick)
        {
            return {};
        }

        if (kind == ListenKind::Track)
        {
            auto const track = FindTrack(*context.Document, id);

            if (track == nullptr || track->IsFolder)
            {
                return {};
            }

            if (context.HearTrack != nullptr && *context.HearTrack)
            {
                std::vector<Note> heard{};
                (*context.HearTrack)(*track, fromTick, toTick, heard);

                std::stable_sort(heard.begin(), heard.end(), [](Note const& a, Note const& b) { return a.Tick < b.Tick; });
                return heard;
            }

            return RenderedNotes(context, *track, fromTick, toTick);
        }

        if (kind == ListenKind::Clip)
        {
            auto const clip = FindClip(*context.Document, id);

            if (clip == nullptr || clip->Length <= 0)
            {
                return {};
            }

            // Side by side: placed at tick 0, and looping when it loops.
            Track side{};
            side.Id = L"listening";
            side.Timeline.push_back(Placement{ std::wstring{ id }, 0, clip->Loop ? INT64_MAX / 4 : clip->Length });
            return RenderedNotes(context, side, fromTick, toTick);
        }

        return {};
    }

    _Use_decl_annotations_
    std::vector<Note> NotesPlayed(ListenContext const& context, Track const& track, int64_t fromTick, int64_t toTick)
    {
        if (context.Document == nullptr || toTick <= fromTick)
        {
            return {};
        }

        return RenderedNotes(context, track, fromTick, toTick);
    }

    _Use_decl_annotations_
    std::vector<LearnedNote> LearnOnce(Sequence const& sequence, int64_t stepTicks, ListenKind kind, std::wstring_view id)
    {
        int64_t end{ 0 };

        if (kind == ListenKind::Clip)
        {
            auto const clip = FindClip(sequence, id);

            if (clip == nullptr || clip->Length <= 0)
            {
                return {};
            }

            // One pass of a clip that's the same each time, and four of one that changes.
            auto const changes = clip->Loop && (clip->Kind == ClipKind::Pattern || (clip->Generator.has_value() && clip->Generator->ChangesEachPass));
            end = clip->Length * (changes ? 4 : 1);
        }
        else if (kind == ListenKind::Track)
        {
            auto const track = FindTrack(sequence, id);

            if (track == nullptr)
            {
                return {};
            }

            end = TrackEndTick(sequence, *track);
        }

        ListenContext const context{ &sequence, nullptr };
        auto const notes = NotesHeard(context, kind, id, 0, std::min(end, MaximumLearnedTicks));

        if (notes.size() < 2)
        {
            return {};
        }

        auto learned = LearnFromNotes(notes, std::max<int64_t>(1, stepTicks));

        if (learned.size() > MaximumLearnedNotes)
        {
            learned.resize(MaximumLearnedNotes);
        }

        return learned.size() >= 2 ? learned : std::vector<LearnedNote>{};
    }

    namespace
    {
        // What a listener heard before a pass, its graph worked out by the caller.
        Hearing HearPass(_In_ ListenContext const& context, _In_ Clip const& clip, _In_ int64_t passStart, _In_ uint64_t pass, _In_ Graph const& graph)
        {
            if (pass == 0)
            {
                return {};
            }

            auto const& generator = *clip.Generator;

            // Past the passes worked out, a pass hears what the last one did, as Life and Evolve play.
            if (pass > MaximumSteppedPasses)
            {
                passStart -= static_cast<int64_t>(pass - MaximumSteppedPasses) * clip.Length;
                pass = MaximumSteppedPasses;
            }

            auto const keyFor = [&](uint64_t earlier) { return HeardKey(clip, earlier, passStart - static_cast<int64_t>(pass - earlier) * clip.Length, graph.Heard); };
            auto const key = keyFor(pass);

            if (auto kept = FindHeard(key); kept.has_value())
            {
                return std::move(*kept);
            }

            if (t_depth >= MaximumDepth)
            {
                return {};
            }

            // In a loop, the passes before this one that aren't kept are worked out first, oldest
            // first, so each finds the one before it kept rather than asking all the way back.
            if (graph.Loops && !graph.Live && pass > 1)
            {
                auto first = pass;

                while (first > 1 && !FindHeard(keyFor(first - 1)).has_value())
                {
                    --first;
                }

                for (auto earlier = first; earlier < pass; ++earlier)
                {
                    (void)HearPass(context, clip, passStart - static_cast<int64_t>(pass - earlier) * clip.Length, earlier, graph);
                }
            }

            // The pass before: side by side for a clip, and where it played for a track.
            auto const from = generator.ListenTo == ListenKind::Clip ? static_cast<int64_t>(pass - 1) * clip.Length : passStart - clip.Length;

            ++t_depth;
            std::vector<Note> notes{};

            try
            {
                notes = NotesHeard(context, generator.ListenTo, generator.ListenId, from, from + clip.Length);
            }
            catch (...)
            {
                --t_depth;
                throw;
            }

            --t_depth;

            Hearing heard{};

            if (notes.size() >= 2)
            {
                auto const stepTicks = std::max<int64_t>(1, generator.StepTicks);
                heard.Notes = StepNotesFrom(notes, from, stepTicks, GeneratorStepCount(clip));
                heard.Learned = LearnFromNotes(notes, stepTicks);

                if (heard.Learned.size() > MaximumLearnedNotes)
                {
                    heard.Learned.resize(MaximumLearnedNotes);
                }

                if (heard.Learned.size() < 2)
                {
                    heard.Learned.clear();
                }
            }

            // Every 32nd pass is kept longer, except what was heard live, which won't be asked for
            // again once playback moves on.
            KeepHeard(key, !graph.Live && pass % KeptEvery == 0, heard);
            return heard;
        }
    }

    _Use_decl_annotations_
    bool CarriesHearing(GeneratorType type) noexcept
    {
        return type == GeneratorType::Life || type == GeneratorType::LifeDrums || type == GeneratorType::Evolve;
    }

    _Use_decl_annotations_
    Hearing HeardBefore(ListenContext const& context, Clip const& clip, int64_t passStart, uint64_t pass)
    {
        if (context.Document == nullptr || !clip.Generator.has_value() || !ListensEachPass(*clip.Generator) || pass == 0 || clip.Length <= 0)
        {
            return {};
        }

        return HearPass(context, clip, passStart, pass, GraphFor(context, clip));
    }

    _Use_decl_annotations_
    PassHearing MakePassHearing(ListenContext const& context, Clip const& clip, int64_t passStart, uint64_t pass)
    {
        PassHearing hearing{};

        if (context.Document == nullptr || !clip.Generator.has_value() || !ListensEachPass(*clip.Generator) || clip.Length <= 0)
        {
            return hearing;
        }

        auto const& generator = *clip.Generator;
        auto const graph = GraphFor(context, clip);

        // A track is heard where the listener plays, so where its first pass starts matters; a clip
        // is heard side by side, the same wherever the listener is. Not clamped: past the passes
        // worked out, HearPass maps a pass back to the last one itself, and the origin has to stay
        // put so the run carries on instead of starting again each pass.
        auto const origin = generator.ListenTo == ListenKind::Track ? passStart - static_cast<int64_t>(pass) * clip.Length : 0;

        Fnv print{};
        print.Add(clip.Id);
        print.Add(static_cast<uint64_t>(clip.Length));
        print.Add(static_cast<uint64_t>(generator.StepTicks));
        print.Add(static_cast<uint64_t>(origin));
        print.Add(graph.Print);
        print.Add(graph.Live ? 1 : 0);

        hearing.RunKey = print.Value;
        hearing.Key = graph.Live ? print.Value ^ (context.Epoch * 0x9E3779B97F4A7C15ull) : print.Value;
        hearing.Live = graph.Live;

        // Used only while the pass it's made for is worked out, which the context and clip outlive.
        hearing.Before = [context, &clip, origin, graph](uint64_t before) -> std::vector<StepNote>
        {
            return HearPass(context, clip, origin + static_cast<int64_t>(before) * clip.Length, before, graph).Notes;
        };

        return hearing;
    }

    _Use_decl_annotations_
    std::vector<StepNote> HearOnce(Sequence const& sequence, Clip const& listener, ListenKind kind, std::wstring_view id)
    {
        if (!listener.Generator.has_value() || listener.Length <= 0 || id.empty())
        {
            return {};
        }

        ListenContext const context{ &sequence, nullptr };
        int64_t from{ 0 };

        if (kind == ListenKind::Track)
        {
            // From the first pass's worth that plays anything, counted from the track's start.
            auto const track = FindTrack(sequence, id);

            if (track == nullptr)
            {
                return {};
            }

            auto const all = NotesHeard(context, kind, id, 0, std::min(TrackEndTick(sequence, *track), MaximumLearnedTicks));

            if (all.empty())
            {
                return {};
            }

            from = all.front().Tick / listener.Length * listener.Length;
        }
        else if (kind != ListenKind::Clip)
        {
            return {};
        }

        auto const notes = NotesHeard(context, kind, id, from, from + listener.Length);

        if (notes.size() < 2)
        {
            return {};
        }

        return StepNotesFrom(notes, from, listener.Generator->StepTicks, GeneratorStepCount(listener));
    }
    _Use_decl_annotations_
    std::wstring ListenSourceName(Sequence const& sequence, ListenKind kind, std::wstring_view id)
    {
        if (kind == ListenKind::Clip)
        {
            auto const clip = FindClip(sequence, id);
            return clip != nullptr ? clip->Name : std::wstring{};
        }

        if (kind == ListenKind::Track)
        {
            auto const track = FindTrack(sequence, id);
            return track != nullptr ? track->Name : std::wstring{};
        }

        return {};
    }
}
