// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// Evolve (design section 28): a genetic algorithm. Each pass makes eight copies of the pass before,
// changes each a little, scores them by plain rules like those in Xinwan Wang's study of
// algorithmic music (small steps, a shape that rises and falls, some syncopation, a motif that
// comes back, an ending on the root) and plays the best. It always moves on to a copy, even one
// that scores lower, so it keeps changing. With a clip or a track to listen to (section 29), it
// starts from what it heard, and a sixth rule, Sounds like, scores how close each copy is to it.
// Nothing is learned from any music outside the song.

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>

namespace midisequencer
{
    namespace
    {
        constexpr uint64_t CheckpointEvery = 32;
        constexpr size_t RunsKept = 6;

        // Passes heard at a time, with the lock let go, before stepping through them.
        constexpr uint64_t HeardAtOnce = 32;

        // Playback hears what plays now, so a pass far past what was worked out hears only this
        // many passes before it, and aims the ones before those at what it learned.
        constexpr uint64_t LiveCatchUp = 64;

        struct Cell
        {
            bool On{ false };
            int32_t Index{ 0 };         // into the key's notes in the range
            uint8_t Loud{ 55 };         // 0 to 100: how loud, which evolves with the notes

            bool operator==(Cell const&) const = default;
        };

        using Melody = std::vector<Cell>;

        struct Run
        {
            uint64_t Key{ 0 };
            uint64_t LastUsed{ 0 };
            uint64_t Pass{ 0 };
            Melody Latest{};
            std::vector<Melody> Checkpoints{};
            std::vector<double> Scores{};
        };

        // What a pass aims to sound like: an index into the ladder for each step, or -1 for a rest.
        // Empty when there's nothing to aim for.
        using Target = std::vector<int32_t>;

        struct Context
        {
            GeneratorSettings const& Generator;
            uint32_t Seed;
            int64_t Steps;
            std::vector<uint8_t> Ladder;

            // What it learned from its source: where it starts, and what it aims for until it hears.
            Target Learned{};

            // What it heard before each pass, when it keeps listening.
            PassHearing const* Hearing{ nullptr };
        };

        // The note of the key and range nearest a number, an octave at a time into the range first,
        // as an index into the ladder: the lower on a tie. -1 for an empty ladder.
        int32_t LadderIndex(_In_ std::vector<uint8_t> const& ladder, _In_ int32_t number)
        {
            if (ladder.empty())
            {
                return -1;
            }

            auto const lowest = static_cast<int32_t>(ladder.front());
            auto const highest = static_cast<int32_t>(ladder.back());

            while (number > highest && number - 12 >= lowest)
            {
                number -= 12;
            }

            while (number < lowest && number + 12 <= highest)
            {
                number += 12;
            }

            int32_t index{ 0 };
            auto nearest = INT32_MAX;

            for (size_t i = 0; i < ladder.size(); ++i)
            {
                auto const distance = std::abs(static_cast<int32_t>(ladder[i]) - number);

                if (distance < nearest)
                {
                    nearest = distance;
                    index = static_cast<int32_t>(i);
                }
            }

            return index;
        }

        // Notes as something to aim for, on their steps: the highest note on a step, as learning
        // keeps a chord's top. Fewer than two notes is nothing to aim for.
        Target TargetFromNotes(_In_ Context const& context, _In_ std::vector<StepNote> const& notes)
        {
            Target target(static_cast<size_t>(context.Steps), -1);
            size_t count{ 0 };

            for (auto const& note : notes)
            {
                auto const step = static_cast<size_t>(((note.Step % context.Steps) + context.Steps) % context.Steps);
                auto const index = LadderIndex(context.Ladder, note.Number);

                if (index > target[step])
                {
                    count += target[step] < 0 ? 1 : 0;
                    target[step] = index;
                }
            }

            return count >= 2 ? target : Target{};
        }

        // What it learned once from its source, from its first note on, as far as a pass goes.
        Target TargetFromLearned(_In_ Context const& context)
        {
            auto const& generator = context.Generator;

            if (generator.ListenTo == ListenKind::None || generator.Learned.size() < 2)
            {
                return {};
            }

            auto const pitches = LearnedPitches(generator, generator.Learned);
            std::vector<StepNote> notes{};
            int64_t step{ 0 };

            for (size_t i = 0; i < generator.Learned.size() && i < pitches.size() && step < context.Steps; ++i)
            {
                notes.push_back(StepNote{ step, generator.Learned[i].Hold, pitches[i], NormalVelocity });
                step += generator.Learned[i].Gap;
            }

            return TargetFromNotes(context, notes);
        }

        // What a pass aims for: what was heard before it, or failing that what it learned. Playback
        // working far ahead aims the passes long before the one it wants at what it learned.
        Target AimFor(_In_ Context const& context, _In_ uint64_t pass, _In_ uint64_t furthest)
        {
            if (context.Hearing != nullptr && pass > 0 && !(context.Hearing->Live && furthest > pass + LiveCatchUp))
            {
                if (auto heard = TargetFromNotes(context, context.Hearing->Before(pass)); !heard.empty())
                {
                    return heard;
                }
            }

            return context.Learned;
        }

        // A target as notes, for drawing.
        std::vector<StepNote> TargetNotes(_In_ Context const& context, _In_ Target const& target)
        {
            std::vector<StepNote> notes{};

            for (size_t step = 0; step < target.size(); ++step)
            {
                if (target[step] >= 0 && static_cast<size_t>(target[step]) < context.Ladder.size())
                {
                    notes.push_back(StepNote{ static_cast<int64_t>(step), 1, context.Ladder[static_cast<size_t>(target[step])], NormalVelocity });
                }
            }

            return notes;
        }

        Melody StartMelody(_In_ Context const& context)
        {
            auto const size = static_cast<int32_t>(context.Ladder.size());
            auto const middle = static_cast<int32_t>(MiddleRootIndex(context.Generator, context.Ladder));
            auto const reach = std::max(1, std::min(size / 2, 7));

            Melody melody(static_cast<size_t>(context.Steps));

            for (int64_t step = 0; step < context.Steps; ++step)
            {
                auto& cell = melody[static_cast<size_t>(step)];
                cell.On = GeneratorRoll(context.Seed, 0, static_cast<uint64_t>(step), 61) % 100 < context.Generator.Busy;
                cell.Index = std::clamp(middle + static_cast<int32_t>(GeneratorRoll(context.Seed, 0, static_cast<uint64_t>(step), 62) % static_cast<uint64_t>(reach * 2 + 1)) - reach, 0, size - 1);

                // Loud on the beat to start with, give or take a little. It evolves from there.
                cell.Loud = static_cast<uint8_t>(std::clamp<int32_t>((step % 4 == 0 ? 80 : 45) + static_cast<int32_t>(GeneratorRoll(context.Seed, 0, static_cast<uint64_t>(step), 64) % 21) - 10, 0, 100));

                // What it learned from its source, when it has one, is where it starts.
                if (static_cast<size_t>(step) < context.Learned.size())
                {
                    cell.On = context.Learned[static_cast<size_t>(step)] >= 0;
                    cell.Index = cell.On ? context.Learned[static_cast<size_t>(step)] : cell.Index;
                }
            }

            return melody;
        }

        // The total, and each rule's score.
        double Score(_In_ Context const& context, _In_ Melody const& melody, _In_ Target const& target, _Out_opt_ std::array<double, EvolveRuleCount>* rules)
        {
            std::array<double, EvolveRuleCount> scores{};

            std::vector<std::pair<int64_t, int32_t>> notes{};

            for (size_t i = 0; i < melody.size(); ++i)
            {
                if (melody[i].On)
                {
                    notes.emplace_back(static_cast<int64_t>(i), melody[i].Index);
                }
            }

            auto const steps = static_cast<double>(std::max<size_t>(1, melody.size()));
            auto const density = std::max(0.0, 1.0 - std::abs(static_cast<double>(notes.size()) / steps - context.Generator.Busy / 100.0) * 2.5);

            if (notes.size() >= 2)
            {
                std::vector<int32_t> moves{};

                for (size_t i = 1; i < notes.size(); ++i)
                {
                    moves.push_back(notes[i].second - notes[i - 1].second);
                }

                // Small steps: a step or two of the scale scores best, a repeat less, a leap least.
                double small{ 0 };
                int32_t ups{ 0 };
                int32_t downs{ 0 };
                int32_t repeats{ 0 };

                for (auto const move : moves)
                {
                    auto const size = std::abs(move);
                    small += size >= 1 && size <= 2 ? 1.0 : size == 0 ? 0.6 : size <= 4 ? 0.3 : 0.0;
                    ups += move > 0 ? 1 : 0;
                    downs += move < 0 ? 1 : 0;
                    repeats += move == 0 ? 1 : 0;
                }

                scores[0] = small / static_cast<double>(moves.size());

                // A shape that rises and falls: as much up as down, turning about a third of the time.
                std::vector<int32_t> directions{};

                for (auto const move : moves)
                {
                    if (move != 0)
                    {
                        directions.push_back(move > 0 ? 1 : -1);
                    }
                }

                int32_t turns{ 0 };

                for (size_t i = 1; i < directions.size(); ++i)
                {
                    turns += directions[i] != directions[i - 1] ? 1 : 0;
                }

                auto const balance = 1.0 - std::abs(ups - downs) / static_cast<double>(std::max(1, ups + downs));
                auto const turning = static_cast<double>(turns) / static_cast<double>(std::max<size_t>(1, directions.size() - 1));
                scores[1] = balance * (1.0 - std::abs(turning - 0.35) / 0.65) * (1.0 - 0.5 * repeats / static_cast<double>(moves.size()));

                // Some syncopation: about a third of the notes off the beat.
                double off{ 0 };

                for (auto const& note : notes)
                {
                    off += note.first % 2 == 1 ? 1.0 : 0.0;
                }

                scores[2] = 1.0 - std::abs(off / static_cast<double>(notes.size()) - 0.3) / 0.7;

                // A motif that comes back: the second half like the first.
                auto const half = melody.size() / 2;
                double same{ 0 };

                for (size_t i = 0; i < half; ++i)
                {
                    auto const& a = melody[i];
                    auto const& b = melody[i + half];
                    same += a.On == b.On && (!a.On || a.Index == b.Index) ? 1.0 : 0.0;
                }

                scores[3] = half > 0 ? same / static_cast<double>(half) : 0.0;

                // Ending on the root, or failing that the fifth.
                auto const last = context.Ladder[static_cast<size_t>(std::clamp<int32_t>(notes.back().second, 0, static_cast<int32_t>(context.Ladder.size()) - 1))];
                auto const root = context.Generator.ScaleRoot % 12;
                scores[4] = last % 12 == root ? 1.0 : last % 12 == (root + 7) % 12 ? 0.5 : 0.0;
            }

            // Sounding like what it heard: playing on the steps it played, near the notes it played.
            if (!target.empty())
            {
                size_t both{ 0 };
                size_t either{ 0 };
                double close{ 0 };

                for (size_t i = 0; i < melody.size() && i < target.size(); ++i)
                {
                    auto const heard = target[i] >= 0;
                    either += melody[i].On || heard ? 1 : 0;

                    if (melody[i].On && heard)
                    {
                        ++both;
                        close += std::max(0.0, 1.0 - std::abs(melody[i].Index - target[i]) / 7.0);
                    }
                }

                auto const rhythm = either > 0 ? static_cast<double>(both) / static_cast<double>(either) : 0.0;
                auto const pitch = both > 0 ? close / static_cast<double>(both) : 0.0;
                scores[5] = rhythm * 0.5 + pitch * 0.5;
            }

            if (rules != nullptr)
            {
                *rules = scores;
            }

            double weighted{ 0 };
            double weights{ 0 };

            for (size_t i = 0; i < context.Generator.Weights.size(); ++i)
            {
                weighted += context.Generator.Weights[i] * scores[i];
                weights += context.Generator.Weights[i];
            }

            if (!target.empty())
            {
                weighted += context.Generator.SoundsLike * scores[5];
                weights += context.Generator.SoundsLike;
            }

            return weights > 0 ? (weighted / weights) * 0.65 + density * 0.35 : density;
        }

        Melody Mutate(_In_ Context const& context, _In_ Melody melody, _In_ uint64_t pass, _In_ size_t child)
        {
            auto const steps = static_cast<int32_t>(melody.size());
            auto const size = static_cast<int32_t>(context.Ladder.size());

            for (uint64_t change = 0; change < context.Generator.Mutations; ++change)
            {
                auto const roll = GeneratorRoll(context.Seed, pass, child * 8 + change, 63);
                auto const at = static_cast<size_t>((roll >> 8) % static_cast<uint64_t>(steps));

                switch (roll % 4)
                {
                case 0:
                {
                    // Move a note up or down a step or two, or play a silent step.
                    static constexpr std::array<int32_t, 4> Moves{ -2, -1, 1, 2 };
                    auto& cell = melody[at];
                    cell.Index = std::clamp(cell.Index + Moves[(roll >> 20) % 4], 0, size - 1);
                    cell.On = true;
                    break;
                }

                case 1:
                    melody[at].On = !melody[at].On;
                    break;

                case 2:
                    std::swap(melody[at], melody[static_cast<size_t>((roll >> 32) % static_cast<uint64_t>(steps))]);
                    break;

                default:
                    // Copy four steps somewhere else, which is how motifs appear.
                    if (steps >= 8)
                    {
                        auto const from = static_cast<size_t>((roll >> 32) % static_cast<uint64_t>(steps - 3));
                        auto const to = static_cast<size_t>((roll >> 8) % static_cast<uint64_t>(steps - 3));

                        for (size_t i = 0; i < 4; ++i)
                        {
                            melody[to + i] = melody[from + i];
                        }
                    }
                    break;
                }

                // Louder or softer too, by a little or a lot, so loud and soft evolve with the notes.
                auto const nudge = GeneratorRoll(context.Seed, pass, child * 8 + change, 65);
                auto& loud = melody[at].Loud;
                loud = static_cast<uint8_t>(std::clamp<int32_t>(static_cast<int32_t>(loud) + static_cast<int32_t>(nudge % 41) - 20, 0, 100));
            }

            return melody;
        }

        struct Generation
        {
            std::array<Melody, EvolveChildren> Children{};
            std::array<double, EvolveChildren> Scores{};
            size_t Chosen{ 0 };
        };

        Generation Breed(_In_ Context const& context, _In_ Melody const& parent, _In_ uint64_t pass, _In_ Target const& target)
        {
            Generation generation{};
            auto best = -1.0;

            for (size_t child = 0; child < EvolveChildren; ++child)
            {
                generation.Children[child] = Mutate(context, parent, pass, child);
                generation.Scores[child] = Score(context, generation.Children[child], target, nullptr);

                if (generation.Scores[child] > best)
                {
                    best = generation.Scores[child];
                    generation.Chosen = child;
                }
            }

            return generation;
        }

        // The melody on a pass, and the scores of the passes up to `scoresWanted`. One that keeps
        // listening aims each pass at what it heard before it. Hearing plays other clips, which in
        // a loop can be this one, so it hears with the lock let go, then steps with the lock held.
        Melody MelodyAt(_In_ Context const& context, _In_ uint64_t pass, _In_ uint64_t scoresWanted, _Out_opt_ std::vector<double>* scores)
        {
            static std::mutex lock{};
            static std::vector<Run> runs{};
            static uint64_t clock{ 0 };

            pass = std::min(pass, MaximumSteppedPasses);
            scoresWanted = std::min(scoresWanted, MaximumSteppedPasses);

            auto const listening = context.Hearing != nullptr;
            auto const key = GeneratorFingerprint(context.Generator, context.Seed, context.Steps) ^ (listening ? (context.Hearing->RunKey | 1) * 0x9E3779B97F4A7C15ull : 0);

            // Scores come from going forward, so a request for them goes at least that far.
            auto const furthest = std::max(pass, scoresWanted > 0 ? scoresWanted - 1 : 0);

            // What each pass aims for, from the first one it steps to.
            std::map<uint64_t, Target> aims{};

            auto const aimAt = [&](uint64_t at) -> Target const*
            {
                if (!listening)
                {
                    return &context.Learned;
                }

                auto const found = aims.find(at);
                return found == aims.end() ? nullptr : &found->second;
            };

            for (;;)
            {
                uint64_t reached{ 0 };
                uint64_t until{ furthest };

                {
                    std::lock_guard guard{ lock };

                    auto run = std::find_if(runs.begin(), runs.end(), [key](Run const& candidate) { return candidate.Key == key; });

                    if (run == runs.end())
                    {
                        Run made{};
                        made.Key = key;
                        made.Latest = StartMelody(context);
                        made.Checkpoints.push_back(made.Latest);
                        made.Scores.push_back(Score(context, made.Latest, context.Learned, nullptr));

                        if (runs.size() >= RunsKept)
                        {
                            auto oldest = std::min_element(runs.begin(), runs.end(), [](Run const& a, Run const& b) { return a.LastUsed < b.LastUsed; });
                            *oldest = std::move(made);
                            run = oldest;
                        }
                        else
                        {
                            runs.push_back(std::move(made));
                            run = runs.end() - 1;
                        }
                    }

                    run->LastUsed = ++clock;

                    auto heardAll = true;

                    while (run->Pass < furthest)
                    {
                        auto const next = run->Pass + 1;
                        auto const aim = aimAt(next);

                        if (aim == nullptr)
                        {
                            heardAll = false;
                            break;
                        }

                        auto const generation = Breed(context, run->Latest, next, *aim);
                        run->Latest = generation.Children[generation.Chosen];
                        run->Pass = next;
                        run->Scores.push_back(generation.Scores[generation.Chosen]);

                        if (next % CheckpointEvery == 0)
                        {
                            run->Checkpoints.push_back(run->Latest);
                        }
                    }

                    if (!heardAll)
                    {
                        reached = run->Pass;
                    }
                    else
                    {
                        if (scores != nullptr)
                        {
                            scores->assign(run->Scores.begin(), run->Scores.begin() + static_cast<ptrdiff_t>(std::min<uint64_t>(scoresWanted, run->Scores.size())));
                        }

                        if (pass == run->Pass)
                        {
                            return run->Latest;
                        }

                        // Back from the last melody kept before the pass.
                        auto const checkpoint = std::min<size_t>(static_cast<size_t>(pass / CheckpointEvery), run->Checkpoints.size() - 1);
                        auto melody = run->Checkpoints[checkpoint];
                        auto at = static_cast<uint64_t>(checkpoint) * CheckpointEvery + 1;

                        for (; at <= pass; ++at)
                        {
                            auto const aim = aimAt(at);

                            if (aim == nullptr)
                            {
                                break;
                            }

                            auto const generation = Breed(context, melody, at, *aim);
                            melody = generation.Children[generation.Chosen];
                        }

                        if (at > pass)
                        {
                            return melody;
                        }

                        reached = at - 1;
                        until = pass;
                    }
                }

                // What the next few passes aim for, with the lock let go. Each time round hears at
                // least one pass more, so this ends.
                for (auto next = reached + 1; next <= std::min(until, reached + HeardAtOnce); ++next)
                {
                    if (!aims.contains(next))
                    {
                        aims.emplace(next, AimFor(context, next, furthest));
                    }
                }
            }
        }

        std::vector<StepNote> Notes(_In_ Context const& context, _In_ Melody const& melody)
        {
            std::vector<StepNote> notes{};

            for (size_t step = 0; step < melody.size(); ++step)
            {
                if (melody[step].On)
                {
                    StepNote note{};
                    note.Step = static_cast<int64_t>(step);
                    note.Number = context.Ladder[static_cast<size_t>(std::clamp<int32_t>(melody[step].Index, 0, static_cast<int32_t>(context.Ladder.size()) - 1))];
                    note.Velocity = VelocityForLoudness(static_cast<double>(melody[step].Loud) / 100.0);
                    notes.push_back(note);
                }
            }

            return notes;
        }

        Context MakeContext(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_opt_ PassHearing const* hearing)
        {
            Context context{ generator, seed, std::clamp<int64_t>(steps, 1, MaximumGeneratorSteps),
                ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote) };
            context.Learned = TargetFromLearned(context);
            context.Hearing = hearing != nullptr && hearing->Before ? hearing : nullptr;
            return context;
        }
    }

    _Use_decl_annotations_
    std::vector<StepNote> EvolvePass(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass, PassHearing const* hearing)
    {
        auto const context = MakeContext(generator, seed, steps, hearing);
        return Notes(context, MelodyAt(context, pass, 0, nullptr));
    }

    _Use_decl_annotations_
    EvolveFamily EvolveFamilyAt(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass, PassHearing const* hearing)
    {
        auto const context = MakeContext(generator, seed, steps, hearing);

        EvolveFamily family{};

        if (!generator.ChangesEachPass)
        {
            pass = 0;
        }

        pass = std::min(pass, MaximumSteppedPasses);

        if (pass == 0)
        {
            auto const start = MelodyAt(context, 0, 0, nullptr);
            family.Parent = Notes(context, start);
            Score(context, start, context.Learned, &family.Rules);
            family.Target = TargetNotes(context, context.Learned);
            return family;
        }

        auto const parent = MelodyAt(context, pass - 1, 0, nullptr);
        auto const aim = AimFor(context, pass, pass);
        auto const generation = Breed(context, parent, pass, aim);

        family.Parent = Notes(context, parent);

        for (size_t child = 0; child < EvolveChildren; ++child)
        {
            family.Children[child] = Notes(context, generation.Children[child]);
            family.Scores[child] = generation.Scores[child];
        }

        family.Chosen = generation.Chosen;
        Score(context, generation.Children[generation.Chosen], aim, &family.Rules);
        family.Target = TargetNotes(context, aim);
        return family;
    }

    _Use_decl_annotations_
    std::vector<double> EvolveScores(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t passes, PassHearing const* hearing)
    {
        auto const context = MakeContext(generator, seed, steps, hearing);
        std::vector<double> scores{};
        MelodyAt(context, 0, passes, &scores);
        return scores;
    }
}
