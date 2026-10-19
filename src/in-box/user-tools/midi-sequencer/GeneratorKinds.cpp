// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "GeneratorKinds.h"
#include "SequenceGenerator.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>

namespace midisequencer
{
    namespace
    {
        // Passes kept, for all the clips that play at once. Playback asks for each pass in pieces.
        constexpr size_t MemoSize = 96;

        struct MemoEntry
        {
            uint64_t Key{ 0 };
            uint64_t Pass{ 0 };
            std::vector<StepNote> Notes{};
        };

        uint64_t Mix(_In_ uint64_t value) noexcept
        {
            value += 0x9E3779B97F4A7C15ull;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
            return value ^ (value >> 31);
        }

        // FNV-1a, a field at a time.
        struct Fingerprint
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

        template <typename T>
        T Clamp(_In_ T value, _In_ T low, _In_ T high) noexcept
        {
            return std::clamp(value, low, high);
        }

        // Each note's velocity in the generator's range: how loud the generator made it, which
        // it wrote in the default range, or one of the shapes every generator offers.
        void ShapeVelocities(_In_ GeneratorSettings const& generator, _In_ uint32_t seed, _In_ int64_t steps, _In_ uint64_t pass, _Inout_ std::vector<StepNote>& notes)
        {
            auto const low = static_cast<double>(std::min(generator.VelocityLow, generator.VelocityHigh));
            auto const high = static_cast<double>(std::max(generator.VelocityLow, generator.VelocityHigh));
            auto const stepTicks = std::max<int64_t>(1, generator.StepTicks);
            auto const beat = static_cast<int64_t>(TicksPerQuarterNote);

            for (auto& note : notes)
            {
                double loudness{ 0.5 };

                switch (generator.VelocityBy)
                {
                case VelocityShape::Steady:
                    break;

                case VelocityShape::Beat:
                {
                    // Counted in 4/4: the bar's first step, each beat, each half beat, the rest.
                    auto const tick = note.Step * stepTicks;
                    loudness = tick % (beat * 4) == 0 ? 1.0 : tick % beat == 0 ? 0.65 : tick % (beat / 2) == 0 ? 0.4 : 0.15;
                    break;
                }

                case VelocityShape::Swell:
                    loudness = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * (static_cast<double>(note.Step) + 0.5) / static_cast<double>(std::max<int64_t>(1, steps)));
                    break;

                case VelocityShape::Random:
                    loudness = static_cast<double>(GeneratorRoll(seed, pass, static_cast<uint64_t>(note.Step) * 128 + note.Number, 90) % 1001) / 1000.0;
                    break;

                default:
                    loudness = (static_cast<double>(note.Velocity) - DefaultVelocityLow) / static_cast<double>(DefaultVelocityHigh - DefaultVelocityLow);
                    break;
                }

                loudness = std::clamp(loudness, 0.0, 1.0);
                note.Velocity = static_cast<uint16_t>(std::max(1.0, std::round(low + loudness * (high - low))));
            }
        }

        // A shape this type doesn't offer, kept from another type or read from a damaged file:
        // this type's own.
        void KeepOfferedVelocityShape(_Inout_ GeneratorSettings& generator)
        {
            if (IsSectionTwentyEightGenerator(generator.Type))
            {
                auto const shapes = VelocityShapesFor(generator.Type);

                if (std::find(shapes.begin(), shapes.end(), generator.VelocityBy) == shapes.end())
                {
                    generator.VelocityBy = DefaultVelocityShape(generator.Type);
                }
            }
        }
    }

    _Use_decl_annotations_
    bool IsSectionTwentyEightGenerator(GeneratorType type) noexcept
    {
        return type >= GeneratorType::Life && type <= GeneratorType::ChangeRinging;
    }

    _Use_decl_annotations_
    std::vector<VelocityShape> VelocityShapesFor(GeneratorType type)
    {
        std::vector<VelocityShape> shapes{};

        switch (type)
        {
        case GeneratorType::Life:
        case GeneratorType::LifeDrums:
            shapes = { VelocityShape::Age, VelocityShape::Crowding };
            break;

        case GeneratorType::Learn:
            shapes = { VelocityShape::Surprise, VelocityShape::Learned };
            break;

        case GeneratorType::Words:
            shapes = { VelocityShape::Speech };
            break;

        case GeneratorType::Evolve:
            shapes = { VelocityShape::Evolved };
            break;

        case GeneratorType::Infinity:
            shapes = { VelocityShape::Fourths, VelocityShape::Distance };
            break;

        case GeneratorType::LSystem:
            shapes = { VelocityShape::Climb, VelocityShape::Depth };
            break;

        case GeneratorType::ChangeRinging:
            shapes = { VelocityShape::Lead, VelocityShape::Strokes, VelocityShape::Treble };
            break;

        default:
            break;
        }

        for (auto const shape : { VelocityShape::Steady, VelocityShape::Beat, VelocityShape::Swell, VelocityShape::Random })
        {
            shapes.push_back(shape);
        }

        return shapes;
    }

    _Use_decl_annotations_
    VelocityShape DefaultVelocityShape(GeneratorType type) noexcept
    {
        switch (type)
        {
        case GeneratorType::Life:
        case GeneratorType::LifeDrums:
            return VelocityShape::Age;

        case GeneratorType::Learn:
            return VelocityShape::Surprise;

        case GeneratorType::Words:
            return VelocityShape::Speech;

        case GeneratorType::Evolve:
            return VelocityShape::Evolved;

        case GeneratorType::Infinity:
            return VelocityShape::Fourths;

        case GeneratorType::LSystem:
            return VelocityShape::Climb;

        case GeneratorType::ChangeRinging:
            return VelocityShape::Lead;

        default:
            return VelocityShape::Steady;
        }
    }

    _Use_decl_annotations_
    bool GeneratorUsesSeed(GeneratorSettings const& generator) noexcept
    {
        switch (generator.Type)
        {
        case GeneratorType::Arpeggiator:
            return generator.ArpOrder == ArpeggiatorOrder::Random;

        case GeneratorType::Life:
        case GeneratorType::LifeDrums:
            // A drawn start plays the same whatever the seed, until cells are added.
            return generator.Scatter || generator.Settled == LifeSettled::AddCells;

        case GeneratorType::Words:
        case GeneratorType::LSystem:
        case GeneratorType::ChangeRinging:
            return false;

        default:
            return true;
        }
    }

    _Use_decl_annotations_
    uint64_t GeneratorRoll(uint32_t seed, uint64_t pass, uint64_t step, uint64_t purpose) noexcept
    {
        // The same as the chance melody's, so the generators share one idea of chance.
        return Mix((static_cast<uint64_t>(seed) << 32) ^ Mix(pass * 0x9E3779B97F4A7C15ull + purpose) ^ (step * 0xBF58476D1CE4E5B9ull));
    }

    GeneratorSettings MakeSectionTwentyEightGenerator(GeneratorType type)
    {
        GeneratorSettings generator{};
        generator.Type = type;
        generator.ChangesEachPass = true;
        generator.NoteLength = 80;

        auto const key = [&generator](uint8_t root, ScaleKind scale, uint8_t low, uint8_t high)
        {
            generator.ScaleRoot = root;
            generator.Scale = scale;
            generator.LowNote = low;
            generator.HighNote = high;
        };

        switch (type)
        {
        case GeneratorType::Life:
            key(9, ScaleKind::MinorPentatonic, 57, 81);
            generator.NoteLength = 90;
            break;

        case GeneratorType::LifeDrums:
            // Channel 10, where General MIDI plays drums.
            generator.Channel = 9;
            generator.Density = 22;
            generator.Voice = LifeVoice::All;
            generator.Join = false;
            generator.NoteLength = 50;
            break;

        case GeneratorType::Learn:
            key(0, ScaleKind::Major, 60, 84);
            generator.Keep = 25;
            generator.NoteLength = 90;
            break;

        case GeneratorType::Words:
            key(7, ScaleKind::MajorPentatonic, 55, 79);
            break;

        case GeneratorType::Evolve:
            key(2, ScaleKind::Dorian, 62, 86);
            generator.Busy = 60;
            break;

        case GeneratorType::Infinity:
            key(4, ScaleKind::NaturalMinor, 52, 76);
            generator.Busy = 100;
            generator.NoteLength = 70;
            break;

        case GeneratorType::LSystem:
            key(7, ScaleKind::MajorPentatonic, 55, 79);
            generator.NoteLength = 85;
            break;

        case GeneratorType::ChangeRinging:
            // Bells ring on, at a pace a tower could keep.
            key(5, ScaleKind::Major, 65, 89);
            generator.StepTicks = TicksPerQuarterNote / 2;
            generator.NoteLength = 300;
            break;

        default:
            break;
        }

        generator.VelocityBy = DefaultVelocityShape(type);
        PrepareGenerator(generator);
        return generator;
    }

    _Use_decl_annotations_
    void PrepareGenerator(GeneratorSettings& generator)
    {
        switch (generator.Type)
        {
        case GeneratorType::LifeDrums:
            if (generator.DrumRows.empty())
            {
                generator.DrumRows = DefaultDrumRows();
            }
            break;

        case GeneratorType::Words:
            if (generator.Text.empty())
            {
                generator.Text = L"Hello, world.";
            }
            break;

        case GeneratorType::Learn:
            if (generator.LearnFrom == LearnSource::Words && generator.Text.empty())
            {
                generator.Text = L"Hello, world.";
            }
            break;

        case GeneratorType::LSystem:
            if (generator.Axiom.empty() || generator.Grammar.empty())
            {
                UseLSystemPreset(generator, 0);
            }
            break;

        default:
            break;
        }

        KeepOfferedVelocityShape(generator);
    }

    _Use_decl_annotations_
    void NormalizeSectionTwentyEight(GeneratorSettings& generator)
    {
        generator.LargestJump = std::min<uint8_t>(generator.LargestJump, 24);

        if (generator.Rule > LifeRule::Seeds) { generator.Rule = LifeRule::Life; }
        if (generator.Voice > LifeVoice::All) { generator.Voice = LifeVoice::Highest; }

        // "Louder" new cells are VelocityShape::Age now, so the old choice plays like the rest.
        if (generator.Births != LifeBirths::Only) { generator.Births = LifeBirths::Plain; }
        if (generator.Settled > LifeSettled::Hold) { generator.Settled = LifeSettled::AddCells; }
        if (generator.VelocityBy > VelocityShape::Treble) { generator.VelocityBy = VelocityShape::Steady; }

        KeepOfferedVelocityShape(generator);

        generator.Density = Clamp<uint8_t>(generator.Density, 1, 90);
        generator.RestartEvery = std::min<uint16_t>(generator.RestartEvery, 999);

        std::erase_if(generator.LifeCells, [](LifeCell const& cell)
        {
            return cell.Column >= MaximumGeneratorSteps || cell.Row >= MaximumLifeRows;
        });

        std::sort(generator.LifeCells.begin(), generator.LifeCells.end());
        generator.LifeCells.erase(std::unique(generator.LifeCells.begin(), generator.LifeCells.end()), generator.LifeCells.end());

        if (generator.DrumRows.size() > MaximumDrumRows)
        {
            generator.DrumRows.resize(MaximumDrumRows);
        }

        for (auto& drum : generator.DrumRows)
        {
            drum &= 0x7F;
        }

        if (generator.LearnFrom > LearnSource::Words) { generator.LearnFrom = LearnSource::Tune; }
        generator.Tune = static_cast<uint8_t>(generator.Tune < TuneCount ? generator.Tune : 0);
        generator.Order = Clamp<uint8_t>(generator.Order, 1, 3);

        if (generator.Learned.size() > MaximumLearnedNotes)
        {
            generator.Learned.resize(MaximumLearnedNotes);
        }

        for (auto& note : generator.Learned)
        {
            note.Gap = Clamp<uint8_t>(note.Gap, 1, 16);
            note.Hold = Clamp<uint8_t>(note.Hold, 1, note.Gap);
            note.Level = std::min<uint8_t>(note.Level, 5);
        }

        if (generator.LearnedFrom.size() > 128) { generator.LearnedFrom.resize(128); }
        if (generator.ListenTo > ListenKind::Track) { generator.ListenTo = ListenKind::None; }
        if (generator.ListenId.size() > 128) { generator.ListenId.clear(); }

        // Listening needs both what it listens to and which one.
        if (generator.ListenTo == ListenKind::None || generator.ListenId.empty())
        {
            generator.ListenTo = ListenKind::None;
            generator.ListenId.clear();
            generator.KeepListening = false;
        }
        if (generator.Text.size() > MaximumGeneratorText) { generator.Text.resize(MaximumGeneratorText); }

        for (auto& weight : generator.Weights)
        {
            weight = std::min<uint8_t>(weight, 100);
        }

        generator.Mutations = Clamp<uint8_t>(generator.Mutations, 1, 4);
        generator.SoundsLike = std::min<uint8_t>(generator.SoundsLike, 100);
        generator.Interval = Clamp<uint8_t>(generator.Interval, 1, 3);
        generator.Voices = Clamp<uint8_t>(generator.Voices, 1, 3);

        if (generator.Axiom.size() > MaximumGrammarLength) { generator.Axiom.resize(MaximumGrammarLength); }

        // One rule a symbol: the first one written wins.
        std::vector<GrammarRule> rules{};

        for (auto& rule : generator.Grammar)
        {
            if (rules.size() < MaximumGrammarRules &&
                std::none_of(rules.begin(), rules.end(), [&rule](GrammarRule const& kept) { return kept.Symbol == rule.Symbol; }))
            {
                if (rule.Replacement.size() > MaximumGrammarLength) { rule.Replacement.resize(MaximumGrammarLength); }
                rules.push_back(rule);
            }
        }

        // Rules apply all at once, so their order doesn't matter: kept by symbol, the way the
        // file's object reads back.
        std::sort(rules.begin(), rules.end(), [](GrammarRule const& a, GrammarRule const& b) { return a.Symbol < b.Symbol; });
        generator.Grammar = std::move(rules);
        generator.Angle = std::min<uint8_t>(generator.Angle, 180);

        generator.Bells = Clamp<uint8_t>(generator.Bells, MinimumBells, MaximumBells);
        if (generator.Method > RingingMethod::Grandsire) { generator.Method = RingingMethod::PlainBob; }
    }

    _Use_decl_annotations_
    bool SameGeneratorSettings(GeneratorSettings const& a, GeneratorSettings const& b) noexcept
    {
        return a.Type == b.Type && a.StepTicks == b.StepTicks && a.ChangesEachPass == b.ChangesEachPass && a.Channel == b.Channel &&
            a.NoteLength == b.NoteLength && a.Rings == b.Rings && a.ScaleRoot == b.ScaleRoot && a.Scale == b.Scale &&
            a.LowNote == b.LowNote && a.HighNote == b.HighNote && a.Busy == b.Busy && a.Keep == b.Keep &&
            a.VelocityLow == b.VelocityLow && a.VelocityHigh == b.VelocityHigh && a.VelocityBy == b.VelocityBy &&
            a.ArpSource == b.ArpSource && a.ArpTrackId == b.ArpTrackId && a.ArpOrder == b.ArpOrder &&
            a.ArpOctaves == b.ArpOctaves && a.ArpLatch == b.ArpLatch &&
            a.LargestJump == b.LargestJump && a.RootEnds == b.RootEnds &&
            a.Rule == b.Rule && a.Scatter == b.Scatter && a.Density == b.Density && a.LifeCells == b.LifeCells &&
            a.Voice == b.Voice && a.Births == b.Births && a.Join == b.Join && a.Settled == b.Settled &&
            a.RestartEvery == b.RestartEvery && a.DrumRows == b.DrumRows &&
            a.LearnFrom == b.LearnFrom && a.Tune == b.Tune && a.Order == b.Order && a.Learned == b.Learned &&
            a.LearnedFrom == b.LearnedFrom && a.Text == b.Text && a.Weights == b.Weights && a.Mutations == b.Mutations && a.SoundsLike == b.SoundsLike &&
            a.Interval == b.Interval && a.Voices == b.Voices && a.Axiom == b.Axiom && a.Grammar == b.Grammar &&
            a.Angle == b.Angle && a.Bells == b.Bells && a.Method == b.Method && a.HandstrokeGap == b.HandstrokeGap &&
            a.ListenTo == b.ListenTo && a.ListenId == b.ListenId && a.KeepListening == b.KeepListening;
    }

    _Use_decl_annotations_
    uint64_t GeneratorFingerprint(GeneratorSettings const& generator, uint32_t seed, int64_t steps) noexcept
    {
        Fingerprint print{};
        print.Add(static_cast<uint64_t>(generator.Type));
        print.Add(seed);
        print.Add(static_cast<uint64_t>(steps));
        print.Add(generator.ChangesEachPass ? 1 : 0);
        print.Add(generator.ScaleRoot);
        print.Add(static_cast<uint64_t>(generator.Scale));
        print.Add(generator.LowNote);
        print.Add(generator.HighNote);
        print.Add(generator.Busy);
        print.Add(generator.Keep);
        print.Add(generator.LargestJump);
        print.Add(generator.RootEnds ? 1 : 0);
        print.Add(static_cast<uint64_t>(generator.Rule));
        print.Add(generator.Scatter ? 1 : 0);
        print.Add(generator.Density);
        print.Add(generator.LifeCells.size());

        for (auto const& cell : generator.LifeCells)
        {
            print.Add((static_cast<uint64_t>(cell.Column) << 16) | cell.Row);
        }

        print.Add(static_cast<uint64_t>(generator.Voice));
        print.Add(static_cast<uint64_t>(generator.Births));
        print.Add(generator.Join ? 1 : 0);
        print.Add(static_cast<uint64_t>(generator.Settled));
        print.Add(generator.RestartEvery);
        print.Add(generator.DrumRows.size());

        for (auto const drum : generator.DrumRows)
        {
            print.Add(drum);
        }

        print.Add(static_cast<uint64_t>(generator.LearnFrom));
        print.Add(generator.Tune);
        print.Add(generator.Order);
        print.Add(generator.Learned.size());

        for (auto const& note : generator.Learned)
        {
            print.Add((static_cast<uint64_t>(note.Level) << 24) | (static_cast<uint64_t>(static_cast<uint8_t>(note.Position)) << 16) | (static_cast<uint64_t>(note.Gap) << 8) | note.Hold);
        }

        print.Add(generator.Text);

        for (auto const weight : generator.Weights)
        {
            print.Add(weight);
        }

        print.Add(generator.Mutations);
        print.Add(generator.SoundsLike);
        print.Add(generator.Interval);
        print.Add(generator.Voices);
        print.Add(generator.Axiom);
        print.Add(generator.Angle);
        print.Add(generator.Grammar.size());

        for (auto const& rule : generator.Grammar)
        {
            print.Add(static_cast<uint64_t>(rule.Symbol));
            print.Add(rule.Replacement);
        }

        print.Add(generator.Bells);
        print.Add(static_cast<uint64_t>(generator.Method));
        print.Add(generator.HandstrokeGap ? 1 : 0);
        print.Add(generator.VelocityLow);
        print.Add(generator.VelocityHigh);
        print.Add(static_cast<uint64_t>(generator.VelocityBy));
        print.Add(static_cast<uint64_t>(generator.ListenTo));
        print.Add(generator.ListenId);
        print.Add(generator.KeepListening ? 1 : 0);

        // How long a step is: the velocity shapes that follow the beat count in it.
        print.Add(static_cast<uint64_t>(generator.StepTicks));
        return print.Value;
    }

    _Use_decl_annotations_
    std::vector<StepNote> GeneratedPass(GeneratorSettings const& generator, uint32_t seed, int64_t steps, uint64_t pass, PassHearing const* hearing)
    {
        static std::mutex lock{};
        static std::vector<MemoEntry> memo{};
        static size_t next{ 0 };

        steps = std::clamp<int64_t>(steps, 1, MaximumGeneratorSteps);

        // Every pass plays the first one's notes, unless the clip changes each pass.
        auto const effective = generator.ChangesEachPass ? std::min(pass, MaximumSteppedPasses) : uint64_t{ 0 };

        // What it heard changes what it plays, so a pass is kept by that too.
        auto const listening = hearing != nullptr && hearing->Before ? hearing : nullptr;
        auto const key = GeneratorFingerprint(generator, seed, steps) ^ (listening != nullptr ? (listening->Key | 1) * 0xD6E8FEB86659FD93ull : 0);

        {
            std::lock_guard guard{ lock };

            for (auto const& entry : memo)
            {
                if (entry.Key == key && entry.Pass == effective)
                {
                    return entry.Notes;
                }
            }
        }

        std::vector<StepNote> notes{};

        switch (generator.Type)
        {
        case GeneratorType::Life:
        case GeneratorType::LifeDrums:
            notes = LifePass(generator, seed, steps, effective, listening);
            break;

        case GeneratorType::Learn:
            notes = LearnPass(generator, seed, steps, effective);
            break;

        case GeneratorType::Words:
            notes = WordsPass(generator, steps, effective);
            break;

        case GeneratorType::Evolve:
            notes = EvolvePass(generator, seed, steps, effective, listening);
            break;

        case GeneratorType::Infinity:
            notes = InfinityPass(generator, seed, steps, effective);
            break;

        case GeneratorType::LSystem:
            notes = LSystemPass(generator, steps, effective);
            break;

        case GeneratorType::ChangeRinging:
            notes = RingingPass(generator, steps, effective);
            break;

        default:
            break;
        }

        std::erase_if(notes, [steps](StepNote const& note) { return note.Step < 0 || note.Step >= steps; });

        ShapeVelocities(generator, seed, steps, effective, notes);

        for (auto& note : notes)
        {
            note.Steps = std::clamp<int64_t>(note.Steps, 1, steps - note.Step);
            note.Number &= 0x7F;
            note.Velocity = std::max<uint16_t>(1, note.Velocity);
        }

        std::stable_sort(notes.begin(), notes.end(), [](StepNote const& a, StepNote const& b)
        {
            return a.Step != b.Step ? a.Step < b.Step : a.Number < b.Number;
        });

        {
            std::lock_guard guard{ lock };

            if (memo.size() < MemoSize)
            {
                memo.push_back(MemoEntry{ key, effective, notes });
            }
            else
            {
                memo[next] = MemoEntry{ key, effective, notes };
                next = (next + 1) % MemoSize;
            }
        }

        return notes;
    }

    // ---------------------------------------------------------------- the key and range

    _Use_decl_annotations_
    size_t MiddleRootIndex(GeneratorSettings const& generator, std::vector<uint8_t> const& ladder)
    {
        if (ladder.empty())
        {
            return 0;
        }

        auto const middle = (static_cast<int32_t>(generator.LowNote) + static_cast<int32_t>(generator.HighNote)) / 2;
        size_t best{ ladder.size() / 2 };
        auto bestGap = INT32_MAX;

        for (size_t i = 0; i < ladder.size(); ++i)
        {
            if (ladder[i] % 12 == generator.ScaleRoot % 12)
            {
                auto const gap = std::abs(static_cast<int32_t>(ladder[i]) - middle);

                if (gap < bestGap)
                {
                    best = i;
                    bestGap = gap;
                }
            }
        }

        return best;
    }

    _Use_decl_annotations_
    size_t ReflectIndex(int64_t index, size_t size) noexcept
    {
        if (size <= 1)
        {
            return 0;
        }

        auto const period = static_cast<int64_t>(size - 1) * 2;
        auto const folded = ((index % period) + period) % period;
        return static_cast<size_t>(folded < static_cast<int64_t>(size) ? folded : period - folded);
    }

    _Use_decl_annotations_
    void ApplyKeyRules(GeneratorSettings const& generator, std::vector<StepNote>& notes)
    {
        if (notes.empty() || (generator.LargestJump == 0 && !generator.RootEnds))
        {
            return;
        }

        auto const ladder = ScaleNotes(generator.ScaleRoot, generator.Scale, generator.LowNote, generator.HighNote);
        auto const jump = static_cast<int32_t>(generator.LargestJump);

        // The root nearest a note, in the range.
        auto const rootNear = [&](int32_t number)
        {
            auto best = number;
            auto bestGap = INT32_MAX;

            for (auto const candidate : ladder)
            {
                if (candidate % 12 == generator.ScaleRoot % 12 && std::abs(static_cast<int32_t>(candidate) - number) < bestGap)
                {
                    best = candidate;
                    bestGap = std::abs(static_cast<int32_t>(candidate) - number);
                }
            }

            return static_cast<uint8_t>(best);
        };

        // The note of the key nearest `wanted` that's no more than the jump from `from`. When the
        // jump is smaller than the scale's steps, the line stays where it is.
        auto const within = [&](int32_t from, int32_t wanted)
        {
            auto chosen = from;
            auto chosenGap = INT32_MAX;

            for (auto const candidate : ladder)
            {
                auto const note = static_cast<int32_t>(candidate);

                if (std::abs(note - from) <= jump && note != from && std::abs(note - wanted) < chosenGap)
                {
                    chosen = note;
                    chosenGap = std::abs(note - wanted);
                }
            }

            return static_cast<uint8_t>(chosen);
        };

        if (generator.RootEnds)
        {
            notes.front().Number = rootNear(notes.front().Number);
        }

        if (jump > 0)
        {
            for (size_t i = 1; i < notes.size(); ++i)
            {
                if (std::abs(static_cast<int32_t>(notes[i].Number) - static_cast<int32_t>(notes[i - 1].Number)) > jump)
                {
                    notes[i].Number = within(notes[i - 1].Number, notes[i].Number);
                }
            }
        }

        if (generator.RootEnds && notes.size() >= 2)
        {
            // The root nearest the note before, and the notes before it bent toward it until the
            // jumps fit again.
            notes.back().Number = rootNear(notes[notes.size() - 2].Number);

            for (auto i = notes.size() - 1; jump > 0 && i >= 2; --i)
            {
                if (std::abs(static_cast<int32_t>(notes[i].Number) - static_cast<int32_t>(notes[i - 1].Number)) <= jump)
                {
                    break;
                }

                notes[i - 1].Number = within(notes[i].Number, notes[i - 1].Number);
            }
        }
    }
}
