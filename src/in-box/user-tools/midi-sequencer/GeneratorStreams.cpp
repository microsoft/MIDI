// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

// The generators that play a stream, a part each pass (design section 28): Per Nørgård's infinity
// series, L-systems, and change ringing's methods.

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <mutex>
#include <tuple>

namespace midisequencer
{
    namespace
    {
        constexpr double Pi = 3.14159265358979323846;

        // An L-system grows until its melody is at least this many steps, or it's this big.
        constexpr int64_t GrowSteps = 3000;
        constexpr int32_t MaximumGenerations = 12;
        constexpr size_t MaximumGrownLength = 200000;

        // A course is never longer than this many rows.
        constexpr size_t MaximumCourseRows = 5000;

        // X, Y and Z only steer how a plant grows. Other capitals play a note, and small letters
        // take a step without one.
        bool TakesAStep(_In_ wchar_t symbol) noexcept
        {
            return ((symbol >= L'A' && symbol <= L'Z') && symbol != L'X' && symbol != L'Y' && symbol != L'Z') ||
                (symbol >= L'a' && symbol <= L'z');
        }

        struct Grown
        {
            uint64_t Key{ 0 };
            uint64_t LastUsed{ 0 };
            TurtleDrawing Drawing{};

            // Each note's Velocity is how loud it is by VelocityShape::Climb. DepthVelocity has
            // the same notes' loudness by VelocityShape::Depth, so either plays without growing
            // the L-system again.
            std::vector<StepNote> Notes{};
            std::vector<uint16_t> DepthVelocity{};
        };

        uint64_t LSystemKey(_In_ GeneratorSettings const& generator) noexcept
        {
            // Only what the L-system and the key and range change.
            GeneratorSettings shape{};
            shape.Type = GeneratorType::LSystem;
            shape.ScaleRoot = generator.ScaleRoot;
            shape.Scale = generator.Scale;
            shape.LowNote = generator.LowNote;
            shape.HighNote = generator.HighNote;
            shape.Axiom = generator.Axiom;
            shape.Grammar = generator.Grammar;
            shape.Angle = generator.Angle;
            return GeneratorFingerprint(shape, 0, 1);
        }

        Grown Grow(_In_ GeneratorSettings const& generator)
        {
            Grown grown{};

            std::wstring text = generator.Axiom;
            int32_t generations{ 0 };

            auto const steps = [](std::wstring const& string)
            {
                return static_cast<int64_t>(std::count_if(string.begin(), string.end(), TakesAStep));
            };

            while (steps(text) < GrowSteps && generations < MaximumGenerations && !generator.Grammar.empty())
            {
                std::wstring next{};

                for (auto const symbol : text)
                {
                    auto const rule = std::find_if(generator.Grammar.begin(), generator.Grammar.end(), [symbol](GrammarRule const& candidate) { return candidate.Symbol == symbol; });
                    next += rule == generator.Grammar.end() ? std::wstring(1, symbol) : rule->Replacement;

                    if (next.size() > MaximumGrownLength)
                    {
                        break;
                    }
                }

                if (next.size() > MaximumGrownLength || next == text)
                {
                    break;
                }

                text = std::move(next);
                ++generations;
            }

            // The turtle: capitals draw, small letters move without drawing, + and - turn, and
            // [ and ] remember and go back. The melody follows how high it is.
            struct Pose
            {
                double X;
                double Y;
                double Heading;
            };

            auto const angle = static_cast<double>(generator.Angle) * Pi / 180.0;
            Pose pose{ 0.0, 0.0, generator.Angle < 60 && generator.Angle > 0 ? Pi / 2.0 : 0.0 };
            std::vector<Pose> stack{};

            auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);
            auto const middle = static_cast<int64_t>(MiddleRootIndex(generator, ladder));

            int64_t step{ 0 };
            auto lastPlayed = false;

            // Where the last note started on the ladder, or -1 before the first.
            int64_t lastIndex{ -1 };

            for (auto const symbol : text)
            {
                if (TakesAStep(symbol))
                {
                    auto const plays = symbol >= L'A' && symbol <= L'Z';
                    auto const index = static_cast<int64_t>(ReflectIndex(middle + static_cast<int64_t>(std::lround(pose.Y)), ladder.size()));
                    auto const number = ladder[static_cast<size_t>(index)];

                    Pose const from = pose;
                    pose.X += std::cos(pose.Heading);
                    pose.Y += std::sin(pose.Heading);

                    grown.Drawing.Lines.push_back(TurtleLine{
                        static_cast<float>(from.X), static_cast<float>(from.Y), static_cast<float>(pose.X), static_cast<float>(pose.Y), step, plays });

                    if (plays)
                    {
                        // The same note again, straight after, holds on.
                        if (lastPlayed && !grown.Notes.empty() && grown.Notes.back().Number == number)
                        {
                            ++grown.Notes.back().Steps;
                        }
                        else
                        {
                            // Louder above the note before and softer below it, so the melody
                            // swells as the turtle climbs and fades as it falls; and softer on
                            // each branch out.
                            auto const rise = lastIndex < 0 ? 0.0 : static_cast<double>(index - lastIndex);
                            auto const climb = std::clamp(0.5 + rise / 4.0, 0.05, 1.0);
                            auto const depth = std::max(0.1, 1.0 - 0.22 * static_cast<double>(stack.size()));
                            grown.Notes.push_back(StepNote{ step, 1, number, VelocityForLoudness(climb) });
                            grown.DepthVelocity.push_back(VelocityForLoudness(depth));
                            lastIndex = index;
                        }
                    }

                    lastPlayed = plays;
                    ++step;
                }
                else if (symbol == L'+')
                {
                    pose.Heading += angle;
                }
                else if (symbol == L'-')
                {
                    pose.Heading -= angle;
                }
                else if (symbol == L'[')
                {
                    stack.push_back(pose);
                }
                else if (symbol == L']' && !stack.empty())
                {
                    pose = stack.back();
                    stack.pop_back();
                    lastPlayed = false;
                }
            }

            grown.Drawing.Generations = generations;
            grown.Drawing.Steps = step;
            return grown;
        }

        Grown GrownFor(_In_ GeneratorSettings const& generator)
        {
            static std::mutex lock{};
            static std::vector<Grown> kept{};
            static uint64_t clock{ 0 };

            auto const key = LSystemKey(generator);

            {
                std::lock_guard guard{ lock };

                for (auto& entry : kept)
                {
                    if (entry.Key == key)
                    {
                        entry.LastUsed = ++clock;
                        return entry;
                    }
                }
            }

            auto grown = Grow(generator);
            grown.Key = key;

            std::lock_guard guard{ lock };
            grown.LastUsed = ++clock;

            if (kept.size() < 4)
            {
                kept.push_back(grown);
            }
            else
            {
                *std::min_element(kept.begin(), kept.end(), [](Grown const& a, Grown const& b) { return a.LastUsed < b.LastUsed; }) = grown;
            }

            return grown;
        }

        // One change: the bells in the places listed stay, and the others swap in pairs. No places
        // at all swaps every pair (a cross).
        void Change(_Inout_ std::vector<uint8_t>& row, _In_ std::initializer_list<uint8_t> places)
        {
            auto const made = [&places](size_t place)
            {
                return std::find(places.begin(), places.end(), static_cast<uint8_t>(place)) != places.end();
            };

            for (size_t place = 1; place < row.size();)
            {
                if (made(place) || made(place + 1))
                {
                    ++place;
                    continue;
                }

                std::swap(row[place - 1], row[place]);
                place += 2;
            }
        }
    }

    // ---------------------------------------------------------------- the infinity series

    _Use_decl_annotations_
    int32_t InfinitySeries(uint64_t index) noexcept
    {
        // From the first 1 bit down: a 1 adds one, a 0 turns it upside down.
        int32_t value{ 0 };
        auto started = false;

        for (int32_t bit = 63; bit >= 0; --bit)
        {
            auto const one = ((index >> bit) & 1) != 0;

            if (!started && !one)
            {
                continue;
            }

            started = true;
            value = one ? value + 1 : -value;
        }

        return value;
    }

    _Use_decl_annotations_
    uint64_t InfinityStart(uint32_t seed) noexcept
    {
        return seed % 4096u;
    }

    _Use_decl_annotations_
    std::vector<StepNote> InfinityPass(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass)
    {
        auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);
        auto const middle = static_cast<int64_t>(MiddleRootIndex(generator, ladder));
        auto const perOctave = static_cast<int64_t>(ScaleNotes(0, generator.Scale, 0, 11).size());
        auto const start = InfinityStart(seed);

        std::vector<StepNote> notes{};

        for (int64_t step = 0; step < steps; ++step)
        {
            auto const global = pass * static_cast<uint64_t>(steps) + static_cast<uint64_t>(step);

            if (GeneratorRoll(seed, 0, global, 71) % 100 < generator.Busy)
            {
                auto const value = InfinitySeries(start + global);
                auto const index = middle + static_cast<int64_t>(value) * generator.Interval;

                // Louder every fourth note, which plays the series again at a quarter of the
                // speed, or the farther it is from its middle.
                auto const loudness = generator.VelocityBy == VelocityShape::Distance
                    ? static_cast<double>(std::abs(value)) / (static_cast<double>(std::abs(value)) + 2.0)
                    : (start + global) % 4 == 0 ? 1.0 : 0.45;

                notes.push_back(StepNote{ step, 1, ladder[ReflectIndex(index, ladder.size())], VelocityForLoudness(loudness) });
            }

            // Each further voice plays the series at half the speed, an octave lower: the same
            // melody the series hides in itself.
            for (uint8_t voice = 1; voice < generator.Voices; ++voice)
            {
                auto const every = uint64_t{ 1 } << voice;

                if (global % every == 0)
                {
                    auto const index = middle - perOctave * voice + static_cast<int64_t>(InfinitySeries(start + global / every)) * generator.Interval;
                    notes.push_back(StepNote{ step, static_cast<int64_t>(every), ladder[ReflectIndex(index, ladder.size())], SoftVelocity });
                }
            }
        }

        return notes;
    }

    // ---------------------------------------------------------------- L-systems

    std::array<LSystemPreset, LSystemPresetCount> const& LSystemPresets() noexcept
    {
        static std::array<LSystemPreset, LSystemPresetCount> const presets{ {
            { L"LSystemDragon", L"FX", { { { L'X', L"X+YF+" }, { L'Y', L"-FX-Y" } } }, 90 },
            { L"LSystemHilbert", L"X", { { { L'X', L"+YF-XFX-FY+" }, { L'Y', L"-XF+YFY+FX-" } } }, 90 },
            { L"LSystemKoch", L"F", { { { L'F', L"F+F-F-F+F" }, { L'\0', nullptr } } }, 90 },
            { L"LSystemPlant", L"X", { { { L'X', L"F+[[X]-X]-F[-FX]+X" }, { L'F', L"FF" } } }, 25 },
            { L"LSystemSierpinski", L"F-G-G", { { { L'F', L"F-G+F+G-F" }, { L'G', L"GG" } } }, 120 },
            { L"LSystemCantor", L"F", { { { L'F', L"FfF" }, { L'f', L"fff" } } }, 0 },
        } };

        return presets;
    }

    _Use_decl_annotations_
    void UseLSystemPreset(GeneratorSettings& generator, size_t preset)
    {
        auto const& chosen = LSystemPresets()[preset < LSystemPresetCount ? preset : 0];

        generator.Axiom = chosen.Axiom;
        generator.Grammar.clear();

        for (auto const& [symbol, replacement] : chosen.Rules)
        {
            if (replacement != nullptr)
            {
                generator.Grammar.push_back(GrammarRule{ symbol, replacement });
            }
        }

        generator.Angle = chosen.Angle;
    }

    _Use_decl_annotations_
    size_t MatchingLSystemPreset(GeneratorSettings const& generator) noexcept
    {
        try
        {
            for (size_t i = 0; i < LSystemPresetCount; ++i)
            {
                GeneratorSettings preset{};
                UseLSystemPreset(preset, i);

                auto sorted = generator.Grammar;
                auto wanted = preset.Grammar;
                auto const bySymbol = [](GrammarRule const& a, GrammarRule const& b) { return a.Symbol < b.Symbol; };
                std::sort(sorted.begin(), sorted.end(), bySymbol);
                std::sort(wanted.begin(), wanted.end(), bySymbol);

                if (generator.Axiom == preset.Axiom && sorted == wanted && generator.Angle == preset.Angle)
                {
                    return i;
                }
            }
        }
        catch (...)
        {
        }

        return SIZE_MAX;
    }

    _Use_decl_annotations_
    TurtleDrawing LSystemTurtle(GeneratorSettings const& generator)
    {
        return GrownFor(generator).Drawing;
    }

    _Use_decl_annotations_
    std::vector<StepNote> LSystemPass(GeneratorSettings const& generator, int64_t steps, uint64_t pass)
    {
        auto const grown = GrownFor(generator);
        auto const length = grown.Drawing.Steps;

        if (length <= 0)
        {
            return {};
        }

        auto const start = length <= steps ? int64_t{ 0 } : static_cast<int64_t>((pass * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length));

        std::vector<StepNote> notes{};
        auto const byDepth = generator.VelocityBy == VelocityShape::Depth && grown.DepthVelocity.size() == grown.Notes.size();

        for (size_t i = 0; i < grown.Notes.size(); ++i)
        {
            auto const& note = grown.Notes[i];
            auto const into = ((note.Step - start) % length + length) % length;

            if (into < steps)
            {
                auto played = note;
                played.Step = into;

                if (byDepth)
                {
                    played.Velocity = grown.DepthVelocity[i];
                }

                notes.push_back(played);
            }
        }

        return notes;
    }

    // ---------------------------------------------------------------- change ringing

    _Use_decl_annotations_
    std::vector<std::vector<uint8_t>> RingingCourse(GeneratorSettings const& generator)
    {
        auto const bells = std::clamp<uint8_t>(generator.Bells, MinimumBells, MaximumBells);
        auto const odd = bells % 2 == 1;

        // A lead's changes, as the places made. An empty list is a cross.
        std::vector<std::vector<uint8_t>> lead{};

        switch (generator.Method)
        {
        case RingingMethod::PlainHunt:
            for (uint8_t i = 0; i < bells; ++i)
            {
                lead.push_back(odd ? std::vector<uint8_t>{ bells } : std::vector<uint8_t>{});
                lead.push_back(odd ? std::vector<uint8_t>{ 1 } : std::vector<uint8_t>{ 1, bells });
            }
            break;

        case RingingMethod::Grandsire:
        {
            // On an odd number of the bells; with an even number, the tenor rings last throughout.
            auto const working = static_cast<uint8_t>(odd ? bells : bells - 1);
            lead.push_back({ 3 });
            lead.push_back({ 1 });

            for (uint8_t i = 1; i < working; ++i)
            {
                lead.push_back({ working });
                lead.push_back({ 1 });
            }
            break;
        }

        case RingingMethod::PlainBob:
        default:
            for (uint8_t i = 0; i < bells; ++i)
            {
                lead.push_back(odd ? std::vector<uint8_t>{ bells } : std::vector<uint8_t>{});
                lead.push_back(odd ? std::vector<uint8_t>{ 1 } : std::vector<uint8_t>{ 1, bells });
            }

            // The lead end: seconds place, which makes the bells work through every lead.
            lead.back() = odd ? std::vector<uint8_t>{ 1, 2, bells } : std::vector<uint8_t>{ 1, 2 };
            break;
        }

        std::vector<uint8_t> rounds(bells);

        for (uint8_t i = 0; i < bells; ++i)
        {
            rounds[i] = static_cast<uint8_t>(i + 1);
        }

        std::vector<std::vector<uint8_t>> rows{ rounds };
        auto row = rounds;

        for (size_t change = 0; change < MaximumCourseRows; ++change)
        {
            auto const& places = lead[change % lead.size()];

            // Change takes the places as a list.
            std::array<uint8_t, 3> made{};
            std::copy_n(places.begin(), std::min<size_t>(places.size(), made.size()), made.begin());

            switch (places.size())
            {
            case 0: Change(row, {}); break;
            case 1: Change(row, { made[0] }); break;
            case 2: Change(row, { made[0], made[1] }); break;
            default: Change(row, { made[0], made[1], made[2] }); break;
            }

            if (row == rounds)
            {
                break;
            }

            rows.push_back(row);
        }

        return rows;
    }

    _Use_decl_annotations_
    int64_t RingingRowStep(GeneratorSettings const& generator, size_t row) noexcept
    {
        auto const bells = static_cast<int64_t>(std::clamp<uint8_t>(generator.Bells, MinimumBells, MaximumBells));
        auto const index = static_cast<int64_t>(row);

        // A handstroke row and a backstroke row, then a step's rest before the next handstroke.
        return generator.HandstrokeGap ? (index / 2) * (bells * 2 + 1) + (index % 2) * bells : index * bells;
    }

    _Use_decl_annotations_
    std::vector<uint8_t> BellNotes(GeneratorSettings const& generator)
    {
        auto const bells = std::clamp<uint8_t>(generator.Bells, MinimumBells, MaximumBells);
        auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, 127);

        // The tenor, the last bell, on the root; the others up the scale from it.
        size_t tenor{ 0 };

        while (tenor < ladder.size() && ladder[tenor] % 12 != generator.ScaleRoot % 12)
        {
            ++tenor;
        }

        if (tenor >= ladder.size())
        {
            tenor = 0;
        }

        std::vector<uint8_t> notes(bells);

        for (uint8_t bell = 1; bell <= bells; ++bell)
        {
            notes[bell - 1] = ladder[std::min(ladder.size() - 1, tenor + static_cast<size_t>(bells - bell))];
        }

        // Listening (design section 29): the bells ring the notes it heard, the most common first,
        // in its key. Bells it heard too few notes for keep the key's notes. The treble stays the
        // highest bell and the tenor the lowest.
        if (generator.ListenTo != ListenKind::None && generator.Learned.size() >= 2)
        {
            auto const heard = LearnedPitches(generator, generator.Learned);

            // Each note heard, how often, and when it was first heard.
            std::vector<std::tuple<size_t, size_t, uint8_t>> counted{};

            for (size_t i = 0; i < heard.size(); ++i)
            {
                auto const same = std::find_if(counted.begin(), counted.end(), [&](auto const& entry) { return std::get<2>(entry) == heard[i]; });

                if (same == counted.end())
                {
                    counted.emplace_back(1, i, heard[i]);
                }
                else
                {
                    ++std::get<0>(*same);
                }
            }

            std::sort(counted.begin(), counted.end(), [](auto const& a, auto const& b)
            {
                return std::get<0>(a) != std::get<0>(b) ? std::get<0>(a) > std::get<0>(b) : std::get<1>(a) < std::get<1>(b);
            });

            std::vector<uint8_t> rung{};

            for (auto const& entry : counted)
            {
                if (rung.size() < bells)
                {
                    rung.push_back(std::get<2>(entry));
                }
            }

            for (auto const note : notes)
            {
                if (rung.size() < bells && std::find(rung.begin(), rung.end(), note) == rung.end())
                {
                    rung.push_back(note);
                }
            }

            std::sort(rung.begin(), rung.end(), std::greater<uint8_t>{});

            if (rung.size() == bells)
            {
                notes = std::move(rung);
            }
        }

        return notes;
    }

    _Use_decl_annotations_
    std::vector<StepNote> RingingPass(GeneratorSettings const& generator, int64_t steps, uint64_t pass)
    {
        auto const course = RingingCourse(generator);
        auto const bells = static_cast<int64_t>(std::clamp<uint8_t>(generator.Bells, MinimumBells, MaximumBells));
        auto const notes = BellNotes(generator);
        auto const length = RingingRowStep(generator, course.size());

        if (length <= 0)
        {
            return {};
        }

        auto const start = static_cast<int64_t>((pass * static_cast<uint64_t>(steps)) % static_cast<uint64_t>(length));
        auto const block = generator.HandstrokeGap ? bells * 2 + 1 : bells;

        std::vector<StepNote> played{};

        for (int64_t step = 0; step < steps; ++step)
        {
            auto const at = (start + step) % length;
            auto const within = at % block;
            auto row = static_cast<size_t>(at / block) * (generator.HandstrokeGap ? 2 : 1);
            auto place = within;

            if (generator.HandstrokeGap)
            {
                if (within == bells * 2)
                {
                    continue;
                }

                if (within >= bells)
                {
                    ++row;
                    place -= bells;
                }
            }

            if (row >= course.size())
            {
                continue;
            }

            auto const bell = course[row][static_cast<size_t>(place)];

            // Louder: the bell leading each row, every handstroke row, or the treble's line.
            double loudness{ 0.45 };

            switch (generator.VelocityBy)
            {
            case VelocityShape::Strokes:
                loudness = row % 2 == 0 ? 0.9 : 0.45;
                break;

            case VelocityShape::Treble:
                loudness = bell == 1 ? 1.0 : 0.45;
                break;

            default:
                loudness = place == 0 ? 1.0 : 0.45;
                break;
            }

            played.push_back(StepNote{ step, 1, notes[static_cast<size_t>(bell - 1)], VelocityForLoudness(loudness) });
        }

        return played;
    }
}
